/*
 * Copyright (c) 2012-2017, Niklas Hauser
 * Copyright (c) 2013, Kevin Läufer
 * Copyright (c) 2014-2017, Sascha Schade
 *
 * This file is part of the modm project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// ----------------------------------------------------------------------------

/*
 * This driver was not so straight forward to implement, because the official
 * documentation by ST is not so clear about the reading operation.
 * So here is the easier to understand version (# = wait for next interrupt).
 *
 * Writing:
 * --------
 *	- set start bit
 *	#
 *	- check SB bit
 *	- write address
 *	#
 *	- check ADDR bit
 *	- clear ADDR bit
 *	- if no bytes are to be written, check for next operation immediately
 *	#
 *	- check TXE bit
 *	- write data
 *	- on write of last bytes, disable Buffer interrupt, and wait for BTF interrupt
 *	#
 *	- check BTF bit
 *	- if no bytes left, check for next operation
 *
 * It is important to note, that we wait for the last byte transfer to complete
 * before checking the next operation.
 *
 *
 * Reading is a lot more complicated. In master read mode, the controller can
 * stretch the SCL line low, while there is new received data in the registers.
 *
 * The data and the shift register together hold two bytes, so we have to send
 * NACK and the STOP condition two bytes in advance and then read both bytes!!!
 *
 * 1-byte read:
 * ------------
 *	- set start bit (RESTART!)
 *	#
 *	- check SB bit
 *	- set ACK low
 *	- write address
 *	#
 *	- check ADDR bit
 *	- clear ADDR bit
 *	- set STOP high
 *	- (wait until STOP low)
 *	- read data 1
 *	- check for next operation
 *
 * 2-byte read:
 * ------------
 *	- set start bit (RESTART!)
 *	#
 *	- check SB bit
 *	- set ACK high
 *	- set POS high (must be used ONLY in two byte transfers, clear it afterwards!)
 *	- write address
 *	#
 *	- check ADDR bit
 *	- clear ADDR bit
 *	- set ACK low
 *	#
 *	- check BTF bit
 *	- set STOP high
 *	- read data 1
 *	- (wait until STOP low)
 *	- read data 2
 *	- check for next operation
 *
 * 3-byte read:
 * ------------
 *	- set start bit (RESTART!)
 *	#
 *	- check SB bit
 *	- write address
 *	#
 *	- check ADDR bit
 *	- clear ADDR bit
 *	#
 *	- check BTF bit
 *	- set ACK LOW
 *	- read data 1
 *	#
 *	- check BTF bit
 *	- set STOP high
 *	- read data 1
 *	- (wait until STOP low)
 *	- read data 2
 *	- check for next operation
 *
 * N-byte read:
 * -------------
 *	- set start bit (RESTART!)
 *	#
 *	- check SB bit
 *	- write address
 *	#
 *	- check ADDR bit
 *	- enable Buffer Interrupt
 *	- clear ADDR bit
 *	#
 *	- check RXNE bit
 *	- read data < N-3
 *	#
 *	- check RXNE bit
 *	- read data N-3
 *	- disable Buffer Interrupt
 *	#
 *	- check BTF bit
 *	- set ACK low
 *	- read data N-2
 *	#
 *	- check BTF bit
 *	- set STOP high
 *	- read data N-1
 *	- (wait until STOP low)
 *	- read data N
 *	- check for next operation
 *
 * Please read the documentation of the driver before you attempt to fix this
 * driver. I strongly recommend the use of an logic analyzer or oscilloscope,
 * to confirm the driver's behavior.
 * The event states are labeled as 'EVn_m' in reference to the manual.
 */

/* To debug the internal state of the driver, you can instantiate a
 * modm::IOStream in your main source file, which will then be used to dump
 * state data of the operations via the serial port, e.g.
 *   #include <modm/io/iostream.hpp>
 *   modm::IODeviceWrapper< Uart5, modm::IOBuffer::BlockIfFull > device;
 *   modm::IOStream stream(device);

 * Be advised, that a typical I2C read/write operation can take 10 to 100 longer
 * because the strings have to be copied during the interrupt!
 *
 * You can enable serial debugging with this define by changing 0 to 1.
 */
#define SERIAL_DEBUGGING 0

#if SERIAL_DEBUGGING
#	include "../../uart/stm32/uart_5.hpp"
	typedef modm::platform::Uart5 DebugUart;
#	include <modm/io/iostream.hpp>
	extern modm::IOStream stream;
#	define DEBUG_STREAM(x) stream << x << "\n"
#	define DEBUG_STREAM_N(x) stream << x;
#	define DEBUG(x) modm::platform::Uart5::write(x)
#else
#	define DEBUG_STREAM(x)
#	define DEBUG_STREAM_N(x)
#	define DEBUG(x)
#endif

#include "i2c_master_1.hpp"
#include <modm/architecture/interface/accessor.hpp>
#include <modm/architecture/interface/atomic_lock.hpp>
#include <modm/architecture/interface/interrupt.hpp>
#include <modm/container.hpp>
#include <modm/platform/clock/rcc.hpp>

MODM_ISR_DECL(I2C1_ER);

namespace
{
	// parameter advice
	static modm::I2c::Operation nextOperation;
	enum class CheckNextOperation : uint8_t
	{
		NO,
		NO_WAIT_FOR_BTF, ///< Do not wait for Byte Transfer Finished
		YES,
		YES_NO_STOP_BIT,
	};

	static CheckNextOperation checkNextOperation = CheckNextOperation::NO;

	// transaction queue management
	struct ConfiguredTransaction
	{
		ConfiguredTransaction()
		:	transaction(nullptr), configuration(nullptr) {}

		ConfiguredTransaction(modm::I2cTransaction *transaction, modm::I2c::ConfigurationHandler configuration)
		:	transaction(transaction), configuration(configuration) {}

		modm::I2cTransaction *transaction;
		modm::I2c::ConfigurationHandler configuration;
	};

	static modm::BoundedQueue<ConfiguredTransaction, 8> queue;
	static modm::I2c::ConfigurationHandler configuration(nullptr);

	// delegating
	static modm::I2cTransaction *transaction(nullptr);
	static modm::I2cMaster::Error error(modm::I2cMaster::Error::NoError);

	// buffer management
	static modm::I2cTransaction::Starting starting(0, modm::I2c::OperationAfterStart::Stop);
	static modm::I2cTransaction::Writing writing(nullptr, 0, modm::I2c::OperationAfterWrite::Stop);
	static modm::I2cTransaction::Reading reading(nullptr, 0, modm::I2c::OperationAfterRead::Stop);

	// helper functions
	static inline void
	callStarting()
	{
		uint_fast32_t deadlockPreventer = 100'000;
		while ((I2C1->CR1 & I2C_CR1_STOP) and (deadlockPreventer-- > 0))
			;

		// If the bus is busy during a starting condition, we generate an error and detach the transaction
		// Before a restart condition the clock line is pulled low, and this check would trigger falsely.
		if ((I2C1->SR2 & I2C_SR2_BUSY) and (nextOperation != modm::I2c::Operation::Restart))
		{
			// we wait a short amount of time for the bus to become free.
			deadlockPreventer = 10'000;
			while ((I2C1->SR2 & I2C_SR2_BUSY) and (deadlockPreventer-- > 0))
				;

			if (I2C1->SR2 & I2C_SR2_BUSY)
			{
				// either SDA or SCL is low, which leads to irrecoverable deadlock.
				// Call error handler manually to detach the transaction object and resolve the deadlock.
				// Further transactions may not succeed either, but will not lead to a deadlock.
				error = modm::I2cMaster::Error::BusBusy;
				MODM_ISR_CALL(I2C1_ER);
				return;
			}
		}

		DEBUG_STREAM("callStarting");
		checkNextOperation = CheckNextOperation::NO;
		error = modm::I2cMaster::Error::NoError;

		I2C1->CR1 &= ~I2C_CR1_POS;
		I2C1->SR1 = 0;
		I2C1->SR2 = 0;

		// and enable interrupts
		DEBUG_STREAM("enable interrupts");
		I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
		I2C1->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITERREN;

		// generate startcondition
		I2C1->CR1 |= I2C_CR1_START;
	}

	static inline void
	callNextTransaction()
	{
		if (queue.isNotEmpty())
		{
			// wait until the stop condition has been generated
			uint_fast32_t deadlockPreventer = 100'000;
			while (I2C1->CR1 & I2C_CR1_STOP && deadlockPreventer-- > 0)
				;

			ConfiguredTransaction next = queue.get();
			queue.pop();
			// configure the peripheral
			if (next.configuration and configuration != next.configuration) {
				configuration = next.configuration;
				configuration();
			}

			DEBUG_STREAM("\n###\n");
			::transaction = next.transaction;
			// start the transaction
			callStarting();
		}
	}
}

// ----------------------------------------------------------------------------
MODM_ISR(I2C1_EV)
{
	DEBUG_STREAM("\n--- interrupt ---");

	uint16_t sr1 = I2C1->SR1;

	if (sr1 & I2C_SR1_SB)
	{
		// Start condition generated.
		// EV5: SB=1, cleared by reading SR1 register followed by writing DR register with Address.
		DEBUG_STREAM("startbit set");

		starting = transaction->starting();
		uint8_t addressMode = modm::I2c::Write;

		switch (starting.next)
		{
			case modm::I2c::OperationAfterStart::Read:
				addressMode = modm::I2c::Read;
				reading = transaction->reading();
				nextOperation = static_cast<modm::I2c::Operation>(reading.next);

				if (reading.length < 2)
				{
					DEBUG_STREAM("NACK");
					I2C1->CR1 &= ~I2C_CR1_ACK;
				}
				else
				{
					DEBUG_STREAM("ACK");
					I2C1->CR1 |= I2C_CR1_ACK;
				}
				if (reading.length == 2)
				{
					DEBUG_STREAM("POS");
					I2C1->CR1 |= I2C_CR1_POS;
				}
				DEBUG_STREAM("read op: reading=" << reading.length);
				break;

			case modm::I2c::OperationAfterStart::Write:
				writing = transaction->writing();
				nextOperation = static_cast<modm::I2c::Operation>(writing.next);

				DEBUG_STREAM("write op: writing=" << writing.length);
				break;

			default:
			case modm::I2c::OperationAfterStart::Stop:
				writing.length = 0;
				reading.length = 0;
				nextOperation = modm::I2c::Operation::Stop;

				DEBUG_STREAM("stop op");
				break;
		}

		I2C1->DR = addressMode | (starting.address & 0xfe);
	}



	else if (sr1 & I2C_SR1_ADDR)
	{
		// End of address transmission
		modm::atomic::Lock lock;
		starting.address = 0;
		// EV6: ADDR=1, cleared by reading SR1 register followed by reading SR2.
		DEBUG_STREAM("address sent");
		DEBUG_STREAM("writing.length=" << writing.length);
		DEBUG_STREAM("reading.length=" << reading.length);

		if ((writing.length > 0) or (reading.length > 3))
		{
			DEBUG_STREAM("enable buffers");
			I2C1->CR2 |= I2C_CR2_ITBUFEN;
		}
		if ((!reading.length) and (!writing.length))
		{
			checkNextOperation = CheckNextOperation::YES;
		}


		DEBUG_STREAM("clearing ADDR");
		(void) I2C1->SR2;
//		uint16_t sr2 = I2C1->SR2;
//		(void) sr2;


		if (reading.length == 1)
		{
			DEBUG_STREAM("STOP");
			I2C1->CR1 |= I2C_CR1_STOP;

			DEBUG_STREAM("waiting for stop");
			uint_fast32_t deadlockPreventer = 100'000;
			while (I2C1->CR1 & I2C_CR1_STOP && deadlockPreventer-- > 0)
				;

			uint16_t dr = I2C1->DR;
			*reading.buffer++ = dr & 0xff;
			reading.length = 0;
			checkNextOperation = CheckNextOperation::YES_NO_STOP_BIT;
		}
		else if (reading.length == 2)
		{
			DEBUG_STREAM("NACK");
			I2C1->CR1 &= ~I2C_CR1_ACK;
		}
	}



	else if (sr1 & I2C_SR1_TXE)
	{
		// EV8_1: TxE=1, shift register empty, data register empty, write Data1 in DR
		// EV8: TxE=1, shift register not empty, data register empty, cleared by writing DR
		if (writing.length > 0)
		{
			DEBUG_STREAM("tx more bytes");
			I2C1->DR = *writing.buffer++; // write data
			--writing.length;

			DEBUG_STREAM("TXE: writing.length=" << writing.length);

			checkNextOperation = CheckNextOperation::NO_WAIT_FOR_BTF;
		}
		// no else!
		if (writing.length == 0)
		{
			// disable TxE, and wait for EV8_2
			DEBUG_STREAM("last byte transmitted, wait for btf");
			I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
		}
	}



	else if (sr1 & I2C_SR1_RXNE)
	{
		// Data register not empty.
		if (reading.length > 3)
		{
			// EV7: RxNE=1, cleared by reading DR register
			uint16_t dr = I2C1->DR;
			*reading.buffer++ = dr & 0xff;
			--reading.length;

			DEBUG_STREAM("RXNE: reading.length=" << reading.length);
		}

		if (reading.length <= 3)
		{
			// disable RxNE, and wait for BTF
			DEBUG_STREAM("fourth last byte received, wait for btf");
			I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
		}
	}



	if (sr1 & I2C_SR1_BTF)
	{
		// EV8_2
		DEBUG_STREAM("BTF");

		if (reading.length == 2)
		{
			modm::atomic::Lock lock;
			// EV7_1: RxNE=1, cleared by reading DR register, programming STOP=1
			DEBUG_STREAM("STOP");
			I2C1->CR1 |= I2C_CR1_STOP;

			DEBUG_STREAM("reading data1");
			uint16_t dr = I2C1->DR;
			*reading.buffer++ = dr & 0xff;

			DEBUG_STREAM("waiting for stop");
			uint_fast32_t deadlockPreventer = 100'000;
			while (I2C1->CR1 & I2C_CR1_STOP && deadlockPreventer-- > 0)
				;

			DEBUG_STREAM("reading data2");
			dr = I2C1->DR;
			*reading.buffer++ = dr & 0xff;

			reading.length = 0;
			checkNextOperation = CheckNextOperation::YES_NO_STOP_BIT;
		}

		if (reading.length == 3)
		{
			// EV7_1: RxNE=1, cleared by reading DR register, programming ACK=0
			I2C1->CR1 &= ~I2C_CR1_ACK;
			DEBUG_STREAM("NACK");

			uint16_t dr = I2C1->DR;
			*reading.buffer++ = dr & 0xff;
			reading.length--;

			DEBUG_STREAM("BTF: reading.length=2");
		}

		if (checkNextOperation == CheckNextOperation::NO_WAIT_FOR_BTF
			&& writing.length == 0)
		{
			// EV8_2: TxE=1, BTF = 1, Program Stop request.
			// TxE and BTF are cleared by hardware by the Stop condition
			DEBUG_STREAM("BTF, write=0");
			checkNextOperation = CheckNextOperation::YES;
		}
	}



	if (checkNextOperation >= CheckNextOperation::YES)
	{
		switch (nextOperation)
		{
			case modm::I2c::Operation::Write:
				if (checkNextOperation != CheckNextOperation::YES_NO_STOP_BIT)
				{
					writing = transaction->writing();
					nextOperation = static_cast<modm::I2c::Operation>(writing.next);
					// reenable TXE
					I2C1->CR2 |= I2C_CR2_ITBUFEN;
					DEBUG_STREAM("write op");
				}
				break;

			case modm::I2c::Operation::Restart:
				callStarting();
				DEBUG_STREAM("restart op");
				break;

			default:
				if (checkNextOperation != CheckNextOperation::YES_NO_STOP_BIT)
				{
					I2C1->CR1 |= I2C_CR1_STOP;
					DEBUG_STREAM("STOP");
				}

				DEBUG_STREAM("disable interrupts");
				I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);
				if (transaction) transaction->detaching(modm::I2c::DetachCause::NormalStop);
				transaction = nullptr;
				DEBUG_STREAM("transaction finished");
				callNextTransaction();
				break;
		}
		checkNextOperation = CheckNextOperation::NO;
	}
}

// ----------------------------------------------------------------------------
MODM_ISR(I2C1_ER)
{
	DEBUG_STREAM("ERROR!");
	uint16_t sr1 = I2C1->SR1;

	if (sr1 & I2C_SR1_BERR)
	{
		DEBUG_STREAM("BUS ERROR");
		I2C1->CR1 |= I2C_CR1_STOP;
		error = modm::I2cMaster::Error::BusCondition;
	}
	else if (sr1 & I2C_SR1_AF)
	{	// acknowledge fail
		I2C1->CR1 |= I2C_CR1_STOP;
		DEBUG_STREAM("ACK FAIL");
		 // may also be ADDRESS_NACK
		error = starting.address ? modm::I2cMaster::Error::AddressNack : modm::I2cMaster::Error::DataNack;
	}
	else if (sr1 & I2C_SR1_ARLO)
	{	// arbitration lost
		DEBUG_STREAM("ARBITRATION LOST");
		error = modm::I2cMaster::Error::ArbitrationLost;
	}
	else if (error == modm::I2cMaster::Error::NoError)
	{
		DEBUG_STREAM("UNKNOWN");
		error = modm::I2cMaster::Error::Unknown;
	}

	if (transaction) transaction->detaching(modm::I2c::DetachCause::ErrorCondition);
	transaction = nullptr;

	// Overrun error is not handled here separately

	// Clear flags and interrupts
	I2C1->CR1 &= ~I2C_CR1_POS;
	I2C1->SR1 = 0;
	I2C1->SR2 = 0;
	writing.length = 0;
	reading.length = 0;
	checkNextOperation = CheckNextOperation::NO;

	DEBUG_STREAM("disable interrupts");
	I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);
	callNextTransaction();
}

// ----------------------------------------------------------------------------

void
modm::platform::I2cMaster1::initializeWithPrescaler(uint8_t peripheralFrequency, uint8_t riseTime, uint16_t prescaler, uint8_t isrPriority)
{
	// no reset, since we want to keep the transaction attached!

	Rcc::enable<Peripheral::I2c1>();

	I2C1->CR1 = I2C_CR1_SWRST; 		// reset module
	I2C1->CR1 = 0;

	NVIC_SetPriority(I2C1_ER_IRQn, isrPriority);
	NVIC_EnableIRQ(I2C1_ER_IRQn);
	NVIC_SetPriority(I2C1_EV_IRQn, isrPriority);
	NVIC_EnableIRQ(I2C1_EV_IRQn);

	I2C1->CR2 = peripheralFrequency;
	I2C1->CCR = prescaler;
	I2C1->TRISE = riseTime;
#ifdef I2C_FLTR_ANOFF_Msk
	// analog filter on, digital filter = 2
	I2C1->FLTR = I2C_FLTR_ANOFF_Msk | 4;
# endif

	I2C1->CR1 |= I2C_CR1_PE; // Enable peripheral
}

void
modm::platform::I2cMaster1::reset()
{
	reading.length = 0;
	writing.length = 0;
	error = Error::SoftwareReset;
	if (transaction) transaction->detaching(DetachCause::ErrorCondition);
	transaction = nullptr;
	// remove all queued transactions
	while(!queue.isEmpty())
	{
		ConfiguredTransaction next = queue.get();
		if (next.transaction) next.transaction->detaching(DetachCause::ErrorCondition);
		queue.pop();
	}
}

// MARK: - ownership
bool
modm::platform::I2cMaster1::start(I2cTransaction *transaction, ConfigurationHandler handler)
{
	modm::atomic::Lock lock;
	// if we have a place in the queue and the transaction object is valid
	if (queue.isNotFull() && transaction)
	{
		// if the transaction object wants to attach to the queue
		if (transaction->attaching())
		{
			// if no current transaction is taking place
			if (!modm::accessor::asVolatile(::transaction))
			{
				// configure the peripheral
				if (handler and configuration != handler) {
					configuration = handler;
					configuration();
				}

				DEBUG_STREAM("\n###\n");
				::transaction = transaction;
				// start the transaction
				callStarting();
			}
			else
			{
				// queue the transaction for later execution
				queue.push(ConfiguredTransaction(transaction, configuration));
			}
			return true;
		}
		else {
			transaction->detaching(modm::I2c::DetachCause::FailedToAttach);
		}
	}
	return false;
}

modm::I2cMaster::Error
modm::platform::I2cMaster1::getErrorState()
{
	return error;
}