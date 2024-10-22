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

using Usart1 = BufferedUart<UsartHal1, UartRxBuffer<32>>;
using Usart3 = BufferedUart<UsartHal3, UartRxBuffer<16>>;
using Usart4 = BufferedUart<UartHal4>;

// ----------------------------------------------------------------------------
modm::IODeviceWrapper<UsbUart0, modm::IOBuffer::BlockIfFull> usb_io_device;

// Set all four logger streams to use the UART
modm::log::Logger modm::log::debug(usb_io_device);
modm::log::Logger modm::log::info(usb_io_device);
modm::log::Logger modm::log::warning(usb_io_device);
modm::log::Logger modm::log::error(usb_io_device);

// ----------------------------------------------------------------------------
modm::amnb::Listener listeners[] = {
	{1,
	 [](uint8_t sender, const uint32_t& data) {
		 MODM_LOG_INFO << "Node2 and Node3 received Broadcast 1 from '" << sender;
		 MODM_LOG_INFO << "': " << data << modm::endl;
	 }},
	{2,
	 [](uint8_t sender) {
		 MODM_LOG_INFO << "Node2 and Node3 received Broadcast 2 from '" << sender << "'"
					   << modm::endl;
	 }},
};
modm::amnb::Action actions[] = {
	{1,
	 []() -> modm::amnb::Response {
		 static uint8_t counter{0};
		 MODM_LOG_INFO << "Node1 and Node3 received Action 1" << modm::endl;
		 return counter++;
	 }},
	{2,
	 [](const uint32_t& data) -> modm::amnb::Response {
		 static uint8_t counter{0};
		 MODM_LOG_INFO << "Node1 and Node3 received Action 2 with argument: " << data << modm::endl;
		 return modm::amnb::ErrorResponse(counter++);
	 }},
};

// Two nodes on the same device on different UARTs of course!
modm::amnb::DeviceWrapper<Usart1> device1;
modm::amnb::DeviceWrapper<Usart3> device2;
modm::amnb::DeviceWrapper<Usart4> device3;
modm::amnb::Node node1(device1, 1, actions);
modm::amnb::Node node2(device2, 2, listeners);
modm::amnb::Node node3(device3, 3, actions, listeners);

// You need to connect D1 with D15 and with A0
using PinNode1 = GpioA9;   // D1
using PinNode2 = GpioC10;  // D15
using PinNode3 = GpioA0;   // A0

void update()
{
	uint32_t counter{0};
	modm::amnb::Result<uint8_t> res1;
	modm::amnb::Result<uint8_t, uint8_t> res2;

	while (true)
	{
		modm::this_fiber::sleep_for(1s);

		Board::LedGreen::toggle();
		node1.broadcast(1, counter++);
		node3.broadcast(2);

		res1 = node2.request<uint8_t>(1, 1);
		MODM_LOG_INFO << "Node1 responded with: " << res1.error();
		if (res1) { MODM_LOG_INFO << " " << *res1 << modm::endl; }

		res2 = node1.request<uint8_t, uint8_t>(3, 2, counter);
		MODM_LOG_INFO << "Node3 responded with: " << res2.error();
		if (res2.hasUserError()) { MODM_LOG_INFO << " " << *res2.userError() << modm::endl; }
	}
}

modm::Fiber<> fiberNode1(+[](){ node1.update(); });
modm::Fiber<> fiberNode2(+[](){ node1.update(); });
modm::Fiber<> fiberNode3(+[](){ node1.update(); });
modm::Fiber<> fiberMangement(update);
modm::Fiber<> fiberUSB(+[](){ tud_task(); Board::LedOrange::toggle(); modm::this_fiber::yield(); });

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

	Usart3::connect<PinNode2::Tx>();
	Usart3::initialize<Board::SystemClock, 115200>(Usart1::Parity::Even, Usart1::WordLength::Bit9);
	// Use Single-Wire Half-Duplex Mode
	PinNode2::configure(Gpio::OutputType::OpenDrain);
	PinNode2::configure(Gpio::InputType::PullUp);
	USART3->CR1 &= ~USART_CR1_UE;
	USART3->CR3 = USART_CR3_HDSEL;
	USART3->CR1 |= USART_CR1_UE;

	Usart4::connect<PinNode3::Tx>();
	Usart4::initialize<Board::SystemClock, 115200>(Usart1::Parity::Even, Usart1::WordLength::Bit9);
	// Use Single-Wire Half-Duplex Mode
	PinNode3::configure(Gpio::OutputType::OpenDrain);
	PinNode3::configure(Gpio::InputType::PullUp);
	UART4->CR1 &= ~USART_CR1_UE;
	UART4->CR3 = USART_CR3_HDSEL;
	UART4->CR1 |= USART_CR1_UE;

	while (true)
	{
		modm::fiber::Scheduler::run();	
	}

	return 0;
}