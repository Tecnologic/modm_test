/*
 * Copyright (c) 2013, Fabian Greif
 * Copyright (c) 2013, Kevin Läufer
 * Copyright (c) 2014-2018, Niklas Hauser
 *
 * This file is part of the modm project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// ----------------------------------------------------------------------------
#include <tusb.h>

#include <modm/board.hpp>
#include <modm/communication/amnb.hpp>
#include <modm/debug.hpp>
#include <modm/processing.hpp>

// ----------------------------------------------------------------------------
// Set the log level
#undef MODM_LOG_LEVEL
#define MODM_LOG_LEVEL modm::log::INFO

using namespace Board;
using namespace std::chrono_literals;
using namespace modm::amnb;
using Usart1 = BufferedUart<UsartHal1, UartRxBuffer<32>>;
using Usart2 = BufferedUart<UsartHal2, UartRxBuffer<16>>;
using Usart3 = BufferedUart<UsartHal3>;

// ----------------------------------------------------------------------------
modm::IODeviceWrapper<UsbUart0, modm::IOBuffer::BlockIfFull> usb_io_device;

// Set all four logger streams to use the UART
modm::log::Logger modm::log::debug(usb_io_device);
modm::log::Logger modm::log::info(usb_io_device);
modm::log::Logger modm::log::warning(usb_io_device);
modm::log::Logger modm::log::error(usb_io_device);

// ----------------------------------------------------------------------------

Listener listeners[] =
{
	{1, [](uint8_t sender, const uint32_t& data)
		{
			MODM_LOG_INFO << "Node2 and Node3 received Broadcast 1 from '" << sender;
			MODM_LOG_INFO << "': " << data << modm::endl;
		}
	},
	{2, [](uint8_t sender)
		{
			MODM_LOG_INFO << "Node2 and Node3 received Broadcast 2 from '" << sender << "'" << modm::endl;
		}
	},
};
Action actions[] =
{
	{1, []() -> Response
		{
			static uint8_t counter{0};
			MODM_LOG_INFO << "Node1 and Node3 received Action 1" << modm::endl;
			return counter++;
		}
	},
	{2, [](const uint32_t& data) -> Response
		{
			static uint8_t counter{0};
			MODM_LOG_INFO << "Node1 and Node3 received Action 2 with argument: " << data << modm::endl;
			LedOrange::toggle();
			return ErrorResponse(counter++);
		}
	},
};

// Two nodes on the same device on different UARTs of course!
DeviceWrapper<Usart1> device1;
DeviceWrapper<Usart2> device2;
DeviceWrapper<Usart3> device3;
Node node1(device1, 1, actions);
Node node2(device2, 2, listeners);
Node node3(device3, 3, actions, listeners);

// You need to connect D1 with D15 and with A0
using PinNode1 = GpioB6;
using PinNode2 = GpioD5;
using PinNode3 = GpioC10;

template< uint8_t Functions >
bool modm::Resumable<Functions>::isResumableRunning(uint8_t id) const
{
	(void)id;
	return false;
}

class Thread
	: public modm::pt::Protothread
{
	modm::ShortPeriodicTimer tmr{1s};
	uint32_t counter{0};
	Result<uint8_t> res1;
	Result<uint8_t, uint8_t> res2;

public:
	bool inline
	update()
	{
		PT_BEGIN();

		while(true)
		{
			PT_WAIT_UNTIL(tmr.execute());

			LedGreen::toggle();

			node1.broadcast(1, counter++);
			node3.broadcast(2);

			res1 = PT_CALL(node2.request<uint8_t>(1, 1));
			MODM_LOG_INFO << "Node1 responded with: " << res1.error();
			if (res1) { MODM_LOG_INFO << " " << *res1 << modm::endl; }

			res2 = PT_CALL(node1.request<uint8_t, uint8_t>(3, 2, counter));
			MODM_LOG_INFO << "Node3 responded with: " << res2.error();
			if (res2.hasUserError()) {
				MODM_LOG_INFO << " " << *res2.userError() << modm::endl;
			}
		}

		PT_END();
	}
}
thread;

// ----------------------------------------------------------------------------
int
main()
{
	Board::initialize();
	Board::initializeUsbFs();

	tusb_init();

	Board::LedOrange::setOutput();
	Board::LedGreen::setOutput();

	Usart1::connect<PinNode1::Tx>();
	Usart1::initialize<Board::SystemClock, 115200>(Usart1::Parity::Even, Usart1::WordLength::Bit9);
	// Use Single-Wire Half-Duplex Mode
	PinNode1::configure(Gpio::OutputType::OpenDrain);
	PinNode1::configure(Gpio::InputType::PullUp);
	USART1->CR1 &= ~USART_CR1_UE;
	USART1->CR3 = USART_CR3_HDSEL;
	USART1->CR1 |= USART_CR1_UE;

	Usart2::connect<PinNode2::Tx>();
	Usart2::initialize<Board::SystemClock, 115200>(Usart1::Parity::Even, Usart1::WordLength::Bit9);
	// Use Single-Wire Half-Duplex Mode
	PinNode2::configure(Gpio::OutputType::OpenDrain);
	PinNode2::configure(Gpio::InputType::PullUp);
	USART2->CR1 &= ~USART_CR1_UE;
	USART2->CR3 = USART_CR3_HDSEL;
	USART2->CR1 |= USART_CR1_UE;

	Usart3::connect<PinNode3::Tx>();
	Usart3::initialize<Board::SystemClock, 115200>(Usart1::Parity::Even, Usart1::WordLength::Bit9);
	// Use Single-Wire Half-Duplex Mode
	PinNode3::configure(Gpio::OutputType::OpenDrain);
	PinNode3::configure(Gpio::InputType::PullUp);
	USART3->CR1 &= ~USART_CR1_UE;
	USART3->CR3 = USART_CR3_HDSEL;
	USART3->CR1 |= USART_CR1_UE;

	while (true)
	{
		tud_task();

		node1.update();
		node2.update();
		node3.update();
		thread.update();
	}

	return 0;
}