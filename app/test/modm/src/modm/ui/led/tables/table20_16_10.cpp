/*
 * Copyright (c) 2009, Martin Rosekeit
 * Copyright (c) 2009-2011, 2017, Fabian Greif
 * Copyright (c) 2012-2015, 2018 Niklas Hauser
 *
 * This file is part of the modm project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// ----------------------------------------------------------------------------

#include "../tables.hpp"

namespace modm
{

namespace ui
{

namespace
{
FLASH_STORAGE(uint16_t table[]) =
{
0x0000,0x0329,0x0ca4,0x1c71,0x3291,0x4f02,0x71c6,0x9adc,0xca44,0xffff,
};
}

modm::accessor::Flash<uint16_t> table20_16_10(table);

}

}

