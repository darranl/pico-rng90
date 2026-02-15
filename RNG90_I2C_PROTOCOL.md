# RNG90 I2C Protocol Reference

## Device Overview

The RNG90 is a NIST-certified random number generator IC that communicates via I2C. It generates 32-byte (256-bit) random numbers with 128-bit security strength.

## I2C Interface Specifications

### Basic Parameters
- **I2C Address**: `0x40` (7-bit)
- **Clock Speed**: Up to 400 kHz (Fast-Mode)
- **Mode**: Client (slave) device only
- **Output Driver**: Open-drain (requires pull-up resistors)
- **Data Format**: MSB first, CRC-16 protected

### Physical Interface
- **SCL Pin**: Serial Clock (input only)
- **SDA Pin**: Serial Data (bidirectional)
- Both pins require external pull-up resistors to VCC

### AC Timing Parameters
| Parameter | Min | Max | Units |
|-----------|-----|-----|-------|
| SCL Frequency | 0 | 400 | kHz |
| SCL High Time | 600 | — | ns |
| SCL Low Time | 1200 | — | ns |
| Data Setup Time | 100 | — | ns |
| Data Hold Time | 0 | — | ns |
| Bus Free Time | 1200 | — | ns |
| Glitch Filter | 50 | 250 | ns |
| Power-up Delay | 1.0–1.8 | — | ms |

## Command Structure

### I/O Groups Format
All commands follow this structure:

```
[Count] [Packet] [CRC-LSB] [CRC-MSB]
```

| Field | Size | Description |
|-------|------|-------------|
| Count | 1 byte | Total group size (4–87 bytes): Count + Packet + 2-byte CRC |
| Packet | N bytes | Opcode + Param1 + Param2[0:1] + optional Data |
| CRC | 2 bytes | CRC-16 checksum (polynomial 0x8005) |

### Command Packet Format
```
[Opcode] [Param1] [Param2-LSB] [Param2-MSB] [Optional Data...]
```

| Field | Size | Description |
|-------|------|-------------|
| Opcode | 1 byte | Command code |
| Param1 | 1 byte | First parameter |
| Param2 | 2 bytes | Second parameter (LSB first) |
| Data | 0+ bytes | Optional data bytes |

## Commands

### Info Command (0x30)
Returns device revision information.

**Input:**
```
Opcode: 0x30
Param1: 0x00 (Revision mode)
Param2: 0x0000
```

**Output (4 bytes):**
```
Byte 0: RFU (0x00)
Byte 1: Device ID (0xD0)
Byte 2: Silicon ID (0x20)
Byte 3: Silicon Rev (0x10)
```

**Execution Time:** 0.28–0.40 ms

### Random Command (0x16)
Generates a 32-byte random number. On first execution after wake/power-up, includes self-tests.

**Input:**
```
Opcode: 0x16
Param1: 0x00
Param2: 0x0000
Data: 20 bytes (any value, must be present)
```

**Output:**
- Success: 32-byte random number
- Failure: 1-byte error code

**Execution Time:**
- First execution: 57.0–72.0 ms (includes self-tests)
- Subsequent: 20.2–25.3 ms

### Read Command (0x02)
Reads device serial number and configuration.

**Input:**
```
Opcode: 0x02
Param1: 0x01 (Read serial number)
Param2: 0x0000
```

**Output (16 bytes):**
```
Bytes [0:8]: Unique 72-bit Device Serial Number
Bytes [9:15]: Reserved/Configuration
```

**Execution Time:** 0.4–0.6 ms

### SelfTest Command (0x77)
Tests device health and DRBG/SHA256 functionality.

**Input:**
```
Opcode: 0x77
Param1: 0x00 (Status), 0x01 (DRBG-SelfTest), 0x20 (SHA256-SelfTest), or 0x21 (DRBG+SHA256-SelfTest)
Param2: 0x0000
```

**Output (1 byte):**

| Response Type | Value | Description |
|---------------|-------|-------------|
| Success | 0x00 | Self-tests successfully run and passed |
| SelfTest Fail | 0x01 | DRBG self-test failed |
| SelfTest Fail | 0x20 | SHA256 self-test failed |
| SelfTest Fail | 0x21 | DRBG and SHA256 self-tests failed |
| SelfTest Not Run | 0x02 | DRBG self-test not run |
| SelfTest Not Run | 0x10 | SHA256 self-test not run |
| SelfTest Not Run | 0x12 | Neither self-test ran |
| Fail | (error code) | Parse/Execution/CRC error |

**Execution Times:**
- Status (0x00): 0.27–0.4 ms
- DRBG-SelfTest (0x01): 25.3–31.8 ms
- SHA256-SelfTest (0x20): 11.4–14.5 ms

**Notes:**
- Initial state after Sleep→Wake or Power-Down→Power-up is `0x12` (neither test ran)
- On failure, PASS/FAIL bits are set; additional `SelfTest` or `Random` runs are required to clear the fail condition
- For consistent `Random` execution time, run `SelfTest` first (the first `Random` call after wake/power-up auto-runs self-tests, increasing its execution time)

## Response Format

Responses from the device share the same I/O group structure as commands:

```
[Count] [Packet] [CRC-LSB] [CRC-MSB]
```

The device does not have a dedicated status register. The output FIFO is shared between status, error, and command results. All outputs are returned as complete groups. The response can be re-read using the `Read` command; each I2C read returns the next sequential byte from the output buffer.

### Status/Error Responses (4 bytes)

When no valid command results are available (error, status, or after wake), the response is always exactly 4 bytes:

```
[Count=0x04] [Status/Error Code] [CRC-LSB] [CRC-MSB]
```

### Data Responses (>4 bytes)

When valid command results are available, the response size is determined by the command:
- **Info**: 7 bytes (count + 4 data bytes + 2 CRC)
- **Random**: 35 bytes (count + 32 data bytes + 2 CRC)
- **Read**: 19 bytes (count + 16 data bytes + 2 CRC)
- **SelfTest**: varies by mode

For Info and Read, success is indicated by the actual output data being present (count > 4), not by a 0x00 success code. For Random and SelfTest, failure returns a 4-byte error response.

## Status/Error Codes

| Code | State | Description |
|------|-------|-------------|
| 0x00 | Success | Command executed successfully |
| 0x03 | Parse Error | Invalid length, opcode, or parameters |
| 0x07 | Self Test Error | DRBG/SHA256 self-test failed |
| 0x08 | Health Test Error | RNG entropy health test failed |
| 0x0F | Execution Error | Command cannot execute in current state |
| 0x11 | Wake Response | Device woke from sleep successfully |
| 0xFF | CRC/Comm Error | Command transmission error, retry required |

### Error Codes by Command

| Error Code | Code | Info | Random | Read | SelfTest |
|------------|------|------|--------|------|----------|
| Success | 0x00 | (1) | (2) | (3) | Yes |
| Parse | 0x03 | Yes | Yes | Yes | Yes |
| SelfTest | 0x07 | — | Yes | (4) | (5) |
| HealthTest | 0x08 | — | Yes | — | (6) |
| Execution Fail | 0x0F | Yes | Yes | Yes | Yes |
| Successful Wake | 0x11 | — | — | — | — |
| Bad CRC | 0xFF | Yes | Yes | Yes | Yes |

**Notes:**
1. Info success is indicated by actual output data, not the 0x00 code
2. Random success returns 32 bytes of random data
3. Read success returns 16 bytes of serial/config data
4. In Self-Test Failure state, Read can still read the serial number without error
5. SelfTest failure is reported as a byte value indicating which test failed
6. Health Tests error can be returned in RNG mode if insufficient entropy
7. Successful Wake (0x11) occurs when the device is first addressed after coming out of Sleep mode

## I2C Bus Transactions

### Write Transaction
```
[START] [ADDR+W] [ACK] [WORD_ADDR] [ACK] [DATA...] [ACK] [STOP]
```

- **ADDR+W**: 0x40 with R/W bit = 0 (write)
- **WORD_ADDR**:
  - `0x00` = Reset address counter
  - `0x01/0x02` = Sleep mode
  - `0x03` = Normal command (standard operation)

### Read Transaction
```
[START] [ADDR+R] [ACK] [DATA...] [NACK] [STOP]
```

- **ADDR+R**: 0x40 with R/W bit = 1 (read)
- Device ACKs each byte until end of transaction
- Host sends NACK to signal end of read

## Device States

### Asleep State
Device draws minimal current (130 nA typical). Ignores all I2C except wake condition.

**Wake Sequence:**
```
[START] [ADDR] [ACK] [STOP] → Wait tPU (1.0–1.8 ms) → Normal operation
```

### Sleep Request
```
[START] [ADDR+W] [ACK] [0x01 or 0x02] [ACK] [STOP]
```
Resets all internal state and volatile registers.

### Busy State
Device ignores all I/O while executing commands. Check status by attempting read:
- Receives ACK: Device is ready, read results
- Receives NACK: Device still busy, retry after ~typical execution time

## CRC Calculation

**Polynomial**: 0x8005 (x¹⁶ + x¹⁵ + x² + x⁰)
**Initial Value**: 0x0000
**Input Data**: Count byte + all Packet bytes (LSB first)
**Output Format**: CRC transmitted as [LSB] [MSB]

### CRC Verification
1. Calculate CRC over Count + Packet bytes
2. Append [CRC_LSB] [CRC_MSB] to transmission
3. Host must also verify CRC on responses

## I2C Synchronization Recovery

If device loses synchronization:

1. **Software Reset Sequence:**
   ```
   [START] [9 SCL cycles with SDA=HIGH] [START] [STOP]
   ```

2. **Reset Address Counter:**
   ```
   [START] [ADDR+W] [ACK] [0x00] [ACK] [STOP]
   ```

3. **If still unresponsive**, device may be asleep:
   - Send wake sequence
   - Wait tPU
   - Retry read/command

## Power Supply and Temperature

- **Operating Voltage**: 1.65V to 5.5V
- **Operating Temperature**: -40°C to +105°C
- **Sleep Current**: 130 nA (typical) to 1000 nA (max)
- **I/O Current**: 60–250 µA (waiting for I/O)
- **Compute Current**: 0.75 mA (during execution)

## Key Implementation Notes

1. **Always include 20 bytes of data with Random command** - parameter must be present even though it doesn't affect output
2. **Multi-byte reads/writes** - can be split across multiple I2C packets if needed
3. **Address counter management** - automatically resets on certain conditions (see datasheet 3.3)
4. **CRC is mandatory** - all commands and responses must include valid 16-bit CRC
5. **Polling required** - after sending command, poll with read attempts until ACK received
6. **First Random call slower** - includes self-tests; subsequent calls are ~3x faster
7. **Health test failures** - can be cleared with SelfTest RNG mode or sleep/wake cycle
