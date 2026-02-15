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

#include "rng90/crc.h"

uint8_t reflect(uint8_t data)
{
    uint8_t reflection = 0x00;
    // Reflect the data about the center bit.
    for (uint8_t bit = 0; bit < 8; ++bit)
    {
        if (data & 0x01)
        {
            reflection |= (1 << (7 - bit));
        }
        data = (data >> 1);
    }
    return reflection;
}

crc_t rng90_crc16(const uint8_t* data, uint8_t length)
{
    crc_t remainder = 0x00;

    for (uint8_t pos = 0; pos < length; ++pos)
    {
        /*
         * Bring the next byte into the remainder.
         */
        remainder ^= (reflect(data[pos]) << 8);

        /*
         * Perform modulo-2 division, a bit at a time.
         */
        for (uint8_t bit = 8; bit > 0; --bit)
        {
            /*
             * Try to divide the current data bit.
             */
            if (remainder & 0x8000) // i.e. is most significant bit set
            {
                remainder = (remainder << 1) ^ POLYNOMIAL;
            }
            else
            {
                remainder = (remainder << 1);
            }
        }
    }

    /*
     * The final remainder is the CRC result.
     */
    remainder = remainder ^ 0x00; // Final XOR Step
    return remainder;
}