/* Copyright 2025, Darran A Lofthouse
 *
 * This file is part of pico-rng90.
 *
 * pico-rng90 is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * pico-rng90 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with pico-rng90.
 * If  not, see <https://www.gnu.org/licenses/>.
 */

#ifndef RNG90_CRC_H
#define RNG90_CRC_H

#include <stdint.h>

#define POLYNOMIAL 0x8005

typedef uint16_t crc_t;

crc_t rng90_crc16(const uint8_t* data, uint8_t length);

#endif // RNG90_CRC_H