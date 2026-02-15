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
#include <stddef.h>
#include <stdint.h>

#include "hardware/i2c.h"

typedef enum {
    RNG90_SELFTEST_STATUS = 0x00,
    RNG90_SELFTEST_DRBG   = 0x01,
    RNG90_SELFTEST_SHA256  = 0x20,
    RNG90_SELFTEST_FULL    = 0x21
} rng90_selftest_type_t;

typedef enum {
    RNG90_SELFTEST_PASSED        = 0x00,
    RNG90_SELFTEST_DRBG_FAILED   = 0x01,
    RNG90_SELFTEST_DRBG_NOT_RUN  = 0x02,
    RNG90_SELFTEST_SHA256_NOT_RUN = 0x10,
    RNG90_SELFTEST_NEITHER_RUN   = 0x12,
    RNG90_SELFTEST_SHA256_FAILED = 0x20,
    RNG90_SELFTEST_BOTH_FAILED   = 0x21,
    RNG90_SELFTEST_COMM_ERROR    = 0xFF
} rng90_selftest_result_t;

struct rng90_context {
    i2c_inst_t* i2c_inst;
    bool initialized;
    bool sleeping;
    uint8_t rfu;
    uint8_t device_id;
    uint8_t silicon_id;
    uint8_t silicon_rev;
    bool test_complete;
    bool logging;
};

typedef struct rng90_context rng90_context_t;

/**
 * Set the I2C instance to be used for communication with the RNG90 device.
 *
 * Additional state on the context will also be rest.
 */
void rng90_set_i2c_instance(rng90_context_t* ctx, i2c_inst_t* i2c_inst);

/**
 * Enable or disable diagnostic logging for the RNG90 driver.
 *
 * Logging is disabled by default. When enabled, I2C commands,
 * responses, and status messages are printed to stdout.
 */
void rng90_set_logging(rng90_context_t* ctx, bool enabled);

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

/**
 * Run or query a self-test on the RNG90 device.
 *
 * If the device is sleeping, it will be woken automatically.
 */
rng90_selftest_result_t rng90_self_test(rng90_context_t* ctx, rng90_selftest_type_t type);

/**
 * Convert a self-test result to a human-readable string.
 */
const char* rng90_selftest_result_str(rng90_selftest_result_t result);

/**
 * Generate random bytes from the RNG90 device.
 *
 * Fills buf with len random bytes, calling the device as many times
 * as necessary (32 bytes per call). If the device is sleeping, it
 * will be woken automatically. Self-test status is checked to
 * determine appropriate timing for the first call.
 *
 * Returns true on success, false on any communication or CRC error.
 */
bool rng90_random(rng90_context_t* ctx, uint8_t* buf, size_t len);

#endif // RNG90_RNG90_H