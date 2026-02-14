/* Copyright 2025, Darran A Lofthouse
 *
 * This file is part of pico-rng90.
 *
 * pico-rng90 is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * pico-playground is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with pico-playground.
 * If  not, see <https://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdio.h>

#include "crc.h"
#include "rng90.h"

#define RNG_90_I2C_ADDRESS 0x40

#define WORD_ADDRESS_RESET 0x00
#define WORD_ADDRESS_SLEEP 0x01
#define WORD_ADDRESS_COMMAND 0x03

#define COMMAND_INFO 0x30

// Internal Function Definitions
bool _rng90_validate_response(const uint8_t* data);
bool _rng90_load_info(rng90_context_t* ctx);
void _rng90_set_crc(uint8_t* data);

void rng90_set_i2c_instance(rng90_context_t* ctx, i2c_inst_t* i2c_inst)
{
    ctx->i2c_inst = i2c_inst;
    ctx->initialized = false;
    ctx->sleeping = true; // Assume sleeping until initialised
    // Other context data reset
    ctx->rfu = 0x00;
    ctx->device_id = 0x00;
    ctx->silicon_id = 0x00;
    ctx->silicon_rev = 0x00;
    ctx->test_complete = false;
}

bool rng90_is_initialized(rng90_context_t* ctx)
{
    return ctx->initialized;
}

bool rng90_is_sleeping(rng90_context_t* ctx)
{
    return ctx->sleeping;
}

/**
 * For initialisation it is possible we all started at the same time,
 * or if just a software reset the RNG90 could have previously been
 * put to sleep so consider both wake and init here.
 */
void rng90_init(rng90_context_t* ctx)
{
    if (ctx->initialized)
    {
        return;
    }

    int count = 0;
    uint8_t command[1] = { WORD_ADDRESS_RESET }; // Reset command, by sending to device it wakes it.
    count = i2c_write_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, command, 1, false);

    if (count < 0)
    {
        // Device may be sleepy.
        sleep_ms(2); // Maximum wake time is 1.8ms
        count = i2c_write_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, command, 1, false);
    }

    if (count < 0)
    {
        // Still failed, give up for now.
        printf("RNG90 I2C wake/init error %d\n", count);
        return;
    }

    printf("RNG90 I2C wake/init wrote %d bytes.\n", count);


    // As the last command was a reset we can read the last response from the device,
    // we probably don't want to try and interpret the conent but may be good to validate
    // the CRC to confirm integrity of the connection.

    // Now read back status to confirm a successful wake.
    uint8_t response[8]; // TODO Could this be longer? e.g. a previous random call (32 Bytes)?
    count = i2c_read_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, response, 1, true);
    if (count < 0)
    {
        printf("RNG90 I2C wake/init read error %d\n", count);
        return;
    }

    printf("RNG90 I2C wake/init read %d bytes. Count 0x%02X\n", count, response[0]);
    count += i2c_read_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, &response[1],
        response[0] < 7 ? response[0] : 7, false);

    // Also maybe better as part of the initialisation and wake routine to
    // verify the device is functioning correctly.
    for (int i = 0; i < count; i++)
    {
        printf("0x%02X ", response[i]);
    }
    printf("\n");

    crc_t crc = rng90_crc16(&response[0], 2);
    printf("LSB 0x%02X\n", crc & 0xFF);
    printf("MSB 0x%02X\n", (crc >> 8) & 0xFF);

    if (!_rng90_validate_response(&response[0]))
    {
        printf("RNG90 I2C wake/init response CRC invalid\n");
        return;
    }

    //   Now we move onto executing the commands we need to learn more about the device.
    //   By reading identification information we further confirm the device is ready for use.
    _rng90_load_info(ctx);

    ctx->sleeping = false;
    ctx->initialized = true;
}

void rng90_sleep(rng90_context_t* ctx) // Immediate?
{
    if (!ctx->initialized || ctx->sleeping)
    {
        return;
    }

    uint8_t command[1] = { 0x01 }; // Sleep command
    int count = i2c_write_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, command, 1, false);

    if (count < 0)
    {
        printf("RNG90 I2C sleep error %d\n", count);
        return;
    }

    printf("RNG90 I2C sleep wrote %d bytes.\n", count);
    ctx->sleeping = true;
}

/*
 * Depending on Clock Divider (which does not seem documented),
 * wake up time could be 1ms 1.2ms, 1.8ms.
 *
 * Maybe commence polling after 1ms.
 */
void rng90_wake();

/**
 * Maybe more useful for us to trigger a wake / detect the device
 * is listening. Reset leaves the device ready for new commands
 * so not a bad option during init.
 */
void rng90_reset();

/**
 * All commands should return immediately if init not performed
 * or device is sleeping.
 */


// Timing Typical = 20.2, Max = 25.3
// Timing Typical (After Boot) = 57, Max = 72
void rng90_random();


// Also maybe better as part of the initialisation and wake routine to
// verify the device is functioning correctly.
// Handling in init / wake will make timing of random more predictable.
// Timing (DRBG-SelfTest) Typical = 25.3, Max = 31.8
// Timing (SHA256-SelfTest)Typical = 11.4, Max = 14.5
// Timing (Status) Typical = 0.27, Max = 0.4
void rng90_selftest();

// Internal function implementations

bool _rng90_validate_response(const uint8_t* data)
{
    // We know the length from byte 0
    uint8_t length = data[0] - 2; // Exclude the CRC bytes.

    crc_t crc = rng90_crc16(&data[0], length);

    uint8_t crc_lsb = crc & 0xFF;
    uint8_t crc_msb = (crc >> 8) & 0xFF;

    if (crc_lsb == data[length] && crc_msb == data[length + 1])
    {
        printf("RNG90 CRC valid.\n");
        return true;
    } else
    {
        printf("RNG90 CRC invalid!\n");
        printf("LSB 0x%02X CRC 0x%02X\n", crc_lsb, data[length]);
        printf("MSB 0x%02X CRC 0x%02X\n", crc_msb, data[length + 1]);

        return false;
    }

}

void _rng90_set_crc(uint8_t* data)
{
    if (!data) return;

    uint8_t total_length = data[0];
    if (total_length < 2) return; // not enough room for CRC

    uint8_t payload_len = total_length - 2; // exclude CRC bytes
    crc_t crc = rng90_crc16(&data[0], payload_len);

    data[payload_len] = (uint8_t)(crc & 0xFF);         // LSB
    data[payload_len + 1] = (uint8_t)((crc >> 8) & 0xFF); // MSB
}

// Maybe not as a distinct function, may capture this data
// as part of init.
// Timing Typical = 0.28, Max = 0.40
// As this function is internal it may be called before the context
// is marked as initialized.
bool _rng90_load_info(rng90_context_t* ctx)
{
    // Command
    // Length 7
    // Info Command
    // Param 1 0x00
    // Param 2 0x00 0x00
    uint8_t info_command[8] = { WORD_ADDRESS_COMMAND, 0x07, COMMAND_INFO, 0x00, 0x00, 0x00, 0x00, 0x00 };
    _rng90_set_crc(&info_command[1]);

    printf("RNG90 Info Command: ");
    for (size_t i = 0; i < sizeof(info_command); ++i) {
        printf("0x%02X ", info_command[i]);
    }
    printf("\n");

    int count = i2c_write_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, info_command, 8, false);
    if (count < 0)
    {
        printf("RNG90 I2C info command write error %d\n", count);
        return false;
    }
    else
    {
        printf("RNG90 I2C info command wrote %d bytes.\n", count);
    }

    printf("RNG90 I2C info command: sleeping 1 ms to wait for response.\n");
    sleep_ms(1); // Typical 280us, Max 400us
    printf("RNG90 I2C info command: wait complete.\n");

    uint8_t length;
    count = i2c_read_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, &length, 1, true);
    if (count < 0)
    {
        printf("RNG90 I2C info command read error %d\n", count);
        return false;
    }
    printf("RNG90 I2C info command length byte read: 0x%02X (%u)\n", length, (unsigned)length);

    uint8_t response[length];
    response[0] = length;
    count += i2c_read_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, &response[1], length - 1, false);
    if (count < 0)
    {
        printf("RNG90 I2C info command read error %d\n", count);
        return false;
    }
    printf("RNG90 I2C info command total bytes read so far: %d\n", count);
    for (int i = 0; i < count; i++) {
        printf("0x%02X ", response[i]);
    }
    printf("\n");

    if (!_rng90_validate_response(&response[0]))
    {
        printf("RNG90 I2C response CRC invalid\n");
        return false;
    }

    if ((unsigned)length < 7) {
        printf("RNG90 I2C info response too short: %u\n", (unsigned)length);
        return false;
    }

    /* Copy first 4 bytes after the length byte into context starting at rfu */
    ctx->rfu = response[1];
    ctx->device_id = response[2];
    ctx->silicon_id = response[3];
    ctx->silicon_rev = response[4];

    printf("RNG90 Info: rfu=0x%02X device=0x%02X silicon=0x%02X rev=0x%02X\n",
           ctx->rfu, ctx->device_id, ctx->silicon_id, ctx->silicon_rev);

    return true;
}