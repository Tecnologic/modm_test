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
0x0000,0x0002,0x000c,0x001d,0x0038,0x005c,0x0089,0x00c0,0x0102,0x014f,0x01a6,0x0209,0x0277,0x02f0,0x0376,0x0407,
0x04a4,0x054e,0x0604,0x06c7,0x0796,0x0872,0x095b,0x0a51,0x0b54,0x0c65,0x0d83,0x0eaf,0x0fe8,0x112e,0x1283,0x13e6,
0x1556,0x16d5,0x1862,0x19fd,0x1ba6,0x1d5e,0x1f24,0x20f9,0x22dc,0x24cf,0x26d0,0x28e0,0x2aff,0x2d2c,0x2f69,0x31b5,
0x3411,0x367b,0x38f5,0x3b7f,0x3e17,0x40c0,0x4377,0x463f,0x4916,0x4bfd,0x4ef4,0x51fa,0x5511,0x5838,0x5b6e,0x5eb5,
0x620b,0x6572,0x68e9,0x6c71,0x7009,0x73b1,0x7769,0x7b32,0x7f0c,0x82f6,0x86f1,0x8afc,0x8f18,0x9345,0x9782,0x9bd1,
0xa030,0xa4a0,0xa921,0xadb4,0xb257,0xb70b,0xbbd0,0xc0a7,0xc58f,0xca88,0xcf92,0xd4ae,0xd9db,0xdf19,0xe469,0xe9ca,
0xef3d,0xf4c1,0xfa57,0xffff,
};
}

modm::accessor::Flash<uint16_t> table22_16_100(table);

}

}

