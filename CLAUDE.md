# pico-rng90

Reusable I2C driver library for the Microchip RNG90 random number generator, targeting Raspberry Pi Pico (RP2040).

## Build

Can be built standalone or included as a subdirectory in another CMake project:

```cmake
add_subdirectory(rng90)
target_link_libraries(your_target rng90)
```

- Build system: CMake (>= 3.13)
- C standard: C11
- SDK: Raspberry Pi Pico SDK (set `PICO_SDK_PATH` for standalone builds)

## Project Structure

- `rng90.c` — Driver implementation
- `crc.c` — CRC-16 implementation (polynomial 0x8005, bit-reflected)
- `include/rng90/rng90.h` — Public driver API
- `include/rng90/crc.h` — Public CRC API
- `RNG90_I2C_PROTOCOL.md` — I2C wire protocol reference

## Public API

All functions operate on a `rng90_context_t` context struct.

| Function | Purpose |
|----------|---------|
| `rng90_set_i2c_instance` | Set I2C instance and reset context |
| `rng90_set_logging` | Enable/disable diagnostic logging |
| `rng90_is_initialized` | Check initialization status |
| `rng90_is_sleeping` | Check sleep status |
| `rng90_init` | Initialize device (wake + load device info) |
| `rng90_sleep` | Put device to sleep |
| `rng90_get_rfu` | Retrieve RFU value |
| `rng90_get_device_id` | Retrieve device ID |
| `rng90_get_silicon_id` | Retrieve silicon ID |
| `rng90_get_silicon_rev` | Retrieve silicon revision |
| `rng90_self_test` | Run or query self-tests (STATUS, DRBG, SHA256, FULL) |
| `rng90_selftest_result_str` | Convert self-test result to string |
| `rng90_random` | Generate random bytes (multi-call, 32 bytes per I2C transaction) |

## Implementation Status

All driver functionality is complete:

- CRC-16 validation on all I2C transactions
- Device init, wake, sleep, and auto-wake
- Device info retrieval (RFU, device ID, silicon ID/rev)
- Self-test execution (4 modes) with timing support
- Random number generation with automatic self-test detection
- Diagnostic logging framework

## I2C Protocol Conventions

- I2C address: 0x40
- All transactions include CRC-16 validation
- CRC bytes are ordered [LSB][MSB] in wire format
- Random command returns 32 bytes per call; driver handles multi-call iteration for larger requests
- Consult `RNG90_I2C_PROTOCOL.md` for detailed wire format

## SDK Dependencies

- `hardware_i2c` — Pico I2C driver (public link dependency)
- `pico_stdlib` — Standard library (private link dependency)

## Conventions

- When implementing new commands, consult `RNG90_I2C_PROTOCOL.md` for the wire format
- Do not auto-commit; ask before committing
- License: GPLv3
