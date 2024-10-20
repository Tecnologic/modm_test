/*
 * Copyright (c) 2009-2010, Martin Rosekeit
 * Copyright (c) 2009-2011, Fabian Greif
 * Copyright (c) 2012-2013, 2015, 2018 Niklas Hauser
 *
 * This file is part of the modm project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// ----------------------------------------------------------------------------

#ifndef MODM_PWM_LED_TABLE_HPP
#define MODM_PWM_LED_TABLE_HPP

#include <stdint.h>
#include <modm/architecture/interface/accessor_flash.hpp>

namespace modm
{

namespace ui
{
	/// 2.0 gamma correction with 8-bit resolution (0 to 255) in 10 steps at 10 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint8_t> table20_8_10;

	/// 2.0 gamma correction with 8-bit resolution (0 to 255) in 100 steps at 100 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint8_t> table20_8_100;

	/// 2.0 gamma correction with 8-bit resolution (0 to 255) in 256 steps at 256 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint8_t> table20_8_256;

	/// 2.0 gamma correction with 14-bit resolution (0 to 16383) in 10 steps at 20 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint16_t> table20_14_10;

	/// 2.0 gamma correction with 14-bit resolution (0 to 16383) in 100 steps at 200 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint16_t> table20_14_100;

	/// 2.0 gamma correction with 14-bit resolution (0 to 16383) in 256 steps at 512 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint16_t> table20_14_256;

	/// 2.0 gamma correction with 16-bit resolution (0 to 65535) in 10 steps at 20 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint16_t> table20_16_10;

	/// 2.0 gamma correction with 16-bit resolution (0 to 65535) in 100 steps at 200 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint16_t> table20_16_100;

	/// 2.0 gamma correction with 16-bit resolution (0 to 65535) in 256 steps at 512 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint16_t> table20_16_256;

	/// 2.2 gamma correction with 8-bit resolution (0 to 255) in 10 steps at 10 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint8_t> table22_8_10;

	/// 2.2 gamma correction with 8-bit resolution (0 to 255) in 100 steps at 100 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint8_t> table22_8_100;

	/// 2.2 gamma correction with 8-bit resolution (0 to 255) in 256 steps at 256 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint8_t> table22_8_256;

	/// 2.2 gamma correction with 14-bit resolution (0 to 16383) in 10 steps at 20 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint16_t> table22_14_10;

	/// 2.2 gamma correction with 14-bit resolution (0 to 16383) in 100 steps at 200 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint16_t> table22_14_100;

	/// 2.2 gamma correction with 14-bit resolution (0 to 16383) in 256 steps at 512 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint16_t> table22_14_256;

	/// 2.2 gamma correction with 16-bit resolution (0 to 65535) in 10 steps at 20 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint16_t> table22_16_10;

	/// 2.2 gamma correction with 16-bit resolution (0 to 65535) in 100 steps at 200 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint16_t> table22_16_100;

	/// 2.2 gamma correction with 16-bit resolution (0 to 65535) in 256 steps at 512 bytes.
	/// @ingroup modm_ui_led
	extern modm::accessor::Flash<uint16_t> table22_16_256;

}

}

#endif	// MODM_PWM_LED_TABLE_HPP