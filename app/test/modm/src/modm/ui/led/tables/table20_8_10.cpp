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
FLASH_STORAGE(uint8_t table[]) =
{
0x0000,0x0003,0x000c,0x001c,0x0032,0x004e,0x0071,0x009a,0x00c9,0x00ff,
};
}

modm::accessor::Flash<uint8_t> table20_8_10(table);

}

}

