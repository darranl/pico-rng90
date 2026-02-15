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

#include <stdint.h>
#include <stdio.h>

#include "rng90/crc.h"
#include "rng90/rng90.h"

#define RNG_90_I2C_ADDRESS 0x40

#define WORD_ADDRESS_RESET 0x00
#define WORD_ADDRESS_SLEEP 0x01
#define WORD_ADDRESS_COMMAND 0x03

#define COMMAND_INFO 0x30
#define COMMAND_SELFTEST 0x77

// Maximum response size: Random command returns 35 bytes (count + 32 data + 2 CRC)
#define MAX_RESPONSE_SIZE 35

// Internal Function Definitions
static bool validate_response(const uint8_t* data);
static bool load_info(rng90_context_t* ctx);
static void set_crc(uint8_t* data);
static void log_message(const char* label, const uint8_t* data, bool is_response);
static bool ensure_awake(rng90_context_t* ctx);

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

uint8_t rng90_get_rfu(rng90_context_t* ctx)
{
    return ctx->rfu;
}

uint8_t rng90_get_device_id(rng90_context_t* ctx)
{
    return ctx->device_id;
}

uint8_t rng90_get_silicon_id(rng90_context_t* ctx)
{
    return ctx->silicon_id;
}

uint8_t rng90_get_silicon_rev(rng90_context_t* ctx)
{
    return ctx->silicon_rev;
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
    uint8_t response[MAX_RESPONSE_SIZE];
    count = i2c_read_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, response, 1, true);
    if (count < 0)
    {
        printf("RNG90 I2C wake/init read error %d\n", count);
        return;
    }

    uint8_t remaining = response[0] < MAX_RESPONSE_SIZE ? response[0] : MAX_RESPONSE_SIZE - 1;
    int read_count = i2c_read_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, &response[1],
        remaining, false);
    if (read_count < 0)
    {
        printf("RNG90 I2C wake/init read error %d\n", read_count);
        return;
    }

    log_message("RNG90 Wake Response:", response, true);

    if (!validate_response(&response[0]))
    {
        printf("RNG90 I2C wake/init response CRC invalid\n");
        return;
    }

    //   Now we move onto executing the commands we need to learn more about the device.
    //   By reading identification information we further confirm the device is ready for use.
    load_info(ctx);

    ctx->sleeping = false;
    ctx->initialized = true;
}

void rng90_sleep(rng90_context_t* ctx)
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

// Internal function implementations

static bool validate_response(const uint8_t* data)
{
    uint8_t length = data[0] - 2; // Exclude the CRC bytes.

    crc_t crc = rng90_crc16(&data[0], length);

    uint8_t crc_lsb = crc & 0xFF;
    uint8_t crc_msb = (crc >> 8) & 0xFF;

    return (crc_lsb == data[length] && crc_msb == data[length + 1]);
}

static void set_crc(uint8_t* data)
{
    if (!data) return;

    uint8_t total_length = data[0];
    if (total_length < 2) return; // not enough room for CRC

    uint8_t payload_len = total_length - 2; // exclude CRC bytes
    crc_t crc = rng90_crc16(&data[0], payload_len);

    data[payload_len] = (uint8_t)(crc & 0xFF);         // LSB
    data[payload_len + 1] = (uint8_t)((crc >> 8) & 0xFF); // MSB
}

static void log_message(const char* label, const uint8_t* data, bool is_response)
{
    uint8_t count = data[0];

    printf("%s Count: 0x%02X (%u)\n", label, count, count);

    // For responses with count == 4, byte 1 is a status/error code
    if (is_response && count == 4)
    {
        uint8_t status = data[1];
        const char* desc;
        switch (status)
        {
            case 0x00: desc = "Success"; break;
            case 0x03: desc = "Parse Error"; break;
            case 0x07: desc = "Self Test Error"; break;
            case 0x08: desc = "Health Test Error"; break;
            case 0x0F: desc = "Execution Error"; break;
            case 0x11: desc = "Wake Response"; break;
            case 0xFF: desc = "CRC/Comm Error"; break;
            default:   desc = "Unknown"; break;
        }
        printf("%s Status: 0x%02X (%s)\n", label, status, desc);
    }
    else
    {
        printf("%s Data:", label);
        for (uint8_t i = 1; i <= count - 3; i++)
        {
            printf(" 0x%02X", data[i]);
        }
        printf("\n");
    }

    uint8_t payload_len = count - 2;
    crc_t crc = rng90_crc16(&data[0], payload_len);
    uint8_t expected_lsb = crc & 0xFF;
    uint8_t expected_msb = (crc >> 8) & 0xFF;
    uint8_t actual_lsb = data[count - 2];
    uint8_t actual_msb = data[count - 1];

    if (expected_lsb == actual_lsb && expected_msb == actual_msb)
    {
        printf("%s CRC: 0x%02X 0x%02X (valid)\n", label, actual_lsb, actual_msb);
    }
    else
    {
        printf("%s CRC: 0x%02X 0x%02X (INVALID - expected 0x%02X 0x%02X)\n",
               label, actual_lsb, actual_msb, expected_lsb, expected_msb);
    }
}

// Timing Typical = 0.28, Max = 0.40
// As this function is internal it may be called before the context
// is marked as initialized.
static bool load_info(rng90_context_t* ctx)
{
    // Command
    // Length 7
    // Info Command
    // Param 1 0x00
    // Param 2 0x00 0x00
    uint8_t info_command[8] = { WORD_ADDRESS_COMMAND, 0x07, COMMAND_INFO, 0x00, 0x00, 0x00, 0x00, 0x00 };
    set_crc(&info_command[1]);

    log_message("RNG90 Info Command:", &info_command[1], false);

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
    uint8_t response[length];
    response[0] = length;
    int read_count = i2c_read_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, &response[1], length - 1, false);
    if (read_count < 0)
    {
        printf("RNG90 I2C info command read error %d\n", read_count);
        return false;
    }

    log_message("RNG90 Info Response:", response, true);

    if (!validate_response(&response[0]))
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

    return true;
}

static bool ensure_awake(rng90_context_t* ctx)
{
    if (!ctx->sleeping)
    {
        return true;
    }

    uint8_t command[1] = { WORD_ADDRESS_RESET };
    int count = i2c_write_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, command, 1, false);

    if (count < 0)
    {
        sleep_ms(2);
        count = i2c_write_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, command, 1, false);
    }

    if (count < 0)
    {
        printf("RNG90 auto-wake error %d\n", count);
        return false;
    }

    uint8_t response[MAX_RESPONSE_SIZE];
    count = i2c_read_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, response, 1, true);
    if (count < 0)
    {
        printf("RNG90 auto-wake read error %d\n", count);
        return false;
    }

    uint8_t remaining = response[0] < MAX_RESPONSE_SIZE ? response[0] : MAX_RESPONSE_SIZE - 1;
    int read_count = i2c_read_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, &response[1],
        remaining, false);
    if (read_count < 0)
    {
        printf("RNG90 auto-wake read error %d\n", read_count);
        return false;
    }

    log_message("RNG90 Auto-Wake Response:", response, true);

    if (!validate_response(response))
    {
        printf("RNG90 auto-wake response CRC invalid\n");
        return false;
    }

    ctx->sleeping = false;
    return true;
}

rng90_selftest_result_t rng90_self_test(rng90_context_t* ctx, rng90_selftest_type_t type)
{
    if (!ctx->initialized)
    {
        printf("RNG90 self_test: not initialized\n");
        return RNG90_SELFTEST_COMM_ERROR;
    }

    if (!ensure_awake(ctx))
    {
        return RNG90_SELFTEST_COMM_ERROR;
    }

    uint8_t command[8] = {
        WORD_ADDRESS_COMMAND, 0x07, COMMAND_SELFTEST,
        (uint8_t)type, 0x00, 0x00, 0x00, 0x00
    };
    set_crc(&command[1]);

    log_message("RNG90 SelfTest Command:", &command[1], false);

    int count = i2c_write_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, command, 8, false);
    if (count < 0)
    {
        printf("RNG90 self_test write error %d\n", count);
        return RNG90_SELFTEST_COMM_ERROR;
    }

    uint32_t wait_ms;
    switch (type)
    {
        case RNG90_SELFTEST_STATUS: wait_ms = 1;  break;
        case RNG90_SELFTEST_DRBG:   wait_ms = 35; break;
        case RNG90_SELFTEST_SHA256:  wait_ms = 16; break;
        case RNG90_SELFTEST_FULL:    wait_ms = 50; break;
        default:                     wait_ms = 50; break;
    }
    sleep_ms(wait_ms);

    uint8_t length;
    count = i2c_read_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS, &length, 1, true);
    if (count < 0)
    {
        printf("RNG90 self_test read error %d\n", count);
        return RNG90_SELFTEST_COMM_ERROR;
    }

    uint8_t response[length];
    response[0] = length;
    int read_count = i2c_read_blocking(ctx->i2c_inst, RNG_90_I2C_ADDRESS,
        &response[1], length - 1, false);
    if (read_count < 0)
    {
        printf("RNG90 self_test read error %d\n", read_count);
        return RNG90_SELFTEST_COMM_ERROR;
    }

    log_message("RNG90 SelfTest Response:", response, true);

    if (!validate_response(response))
    {
        printf("RNG90 self_test response CRC invalid\n");
        return RNG90_SELFTEST_COMM_ERROR;
    }

    return (rng90_selftest_result_t)response[1];
}

const char* rng90_selftest_result_str(rng90_selftest_result_t result)
{
    switch (result)
    {
        case RNG90_SELFTEST_PASSED:        return "All tests passed";
        case RNG90_SELFTEST_DRBG_FAILED:   return "DRBG self-test failed";
        case RNG90_SELFTEST_DRBG_NOT_RUN:  return "DRBG self-test not run";
        case RNG90_SELFTEST_SHA256_NOT_RUN: return "SHA256 self-test not run";
        case RNG90_SELFTEST_NEITHER_RUN:   return "Neither self-test run";
        case RNG90_SELFTEST_SHA256_FAILED: return "SHA256 self-test failed";
        case RNG90_SELFTEST_BOTH_FAILED:   return "DRBG and SHA256 self-tests failed";
        case RNG90_SELFTEST_COMM_ERROR:    return "Communication error";
        default:                           return "Unknown result";
    }
}