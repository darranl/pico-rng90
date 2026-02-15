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

#ifndef RNG90_RNG90_H
#define RNG90_RNG90_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/i2c.h"

struct rng90_context {
    i2c_inst_t* i2c_inst;
    bool initialized;
    bool sleeping;
    uint8_t rfu;
    uint8_t device_id;
    uint8_t silicon_id;
    uint8_t silicon_rev;
    bool test_complete;
};

typedef struct rng90_context rng90_context_t;

/**
 * Set the I2C instance to be used for communication with the RNG90 device.
 *
 * Additional state on the context will also be rest.
 */
void rng90_set_i2c_instance(rng90_context_t* ctx, i2c_inst_t* i2c_inst);

/**
 * Check if the RNG90 context has been initialized.
 */
bool rng90_is_initialized(rng90_context_t* ctx);

/**
 * Check if the RNG90 device is currently sleeping.
 */
bool rng90_is_sleeping(rng90_context_t* ctx);

/**
 * Initialize the RNG90 device.
 *
 * Wake up the device if sleeping and load info
 * from device.
 */
void rng90_init(rng90_context_t* ctx);

/**
 * Put the RNG90 device to sleep.
 */
void rng90_sleep(rng90_context_t* ctx);

/**
 * Get the RFU (Reserved for Future Use) value from the device info.
 */
uint8_t rng90_get_rfu(rng90_context_t* ctx);

/**
 * Get the Device ID from the device info.
 */
uint8_t rng90_get_device_id(rng90_context_t* ctx);

/**
 * Get the Silicon ID from the device info.
 */
uint8_t rng90_get_silicon_id(rng90_context_t* ctx);

/**
 * Get the Silicon Revision from the device info.
 */
uint8_t rng90_get_silicon_rev(rng90_context_t* ctx);

#endif // RNG90_RNG90_H