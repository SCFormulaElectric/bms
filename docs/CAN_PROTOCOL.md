# BMS CAN telemetry

Bus speed is 500 kbit/s. Frames use standard 11-bit identifiers and
little-endian multibyte fields. Incoming CAN is diagnostic-only and cannot
directly command a safety output.

## One BMS namespace

All transmitted identifiers come from `CAN_BMS_BASE_ID` in
`Core/Inc/Peripherals/can_protocol.h`. The base must be aligned to `0xN00`, so
the default `0x600` reserves the `0x6XX` namespace for this BMS. No BMS source
file owns an independent absolute CAN ID.

To move the complete BMS namespace, change only:

```c
#define CAN_BMS_BASE_ID 0x600U
```

Each message then adds its fixed subitem offset:

| Offset | Default ID | Enable-mask name | Period | Payload |
|---:|---:|---|---:|---|
| `0x00` | `0x600` | `CAN_BMS_MSG_HEARTBEAT` | 100 ms | state u8, rolling counter u8, SOC permille u16, sample time ms u32 |
| `0x01` | `0x601` | `CAN_BMS_MSG_PACK_STATUS` | 100 ms | pack voltage mV i32, pack current mA i32 |
| `0x02` | `0x602` | `CAN_BMS_MSG_CELL_EXTREMA` | 100 ms | min/max cell mV u16/u16, min/max indices u8/u8, SOC permille u16 |
| `0x03` | `0x603` | `CAN_BMS_MSG_TEMPERATURES` | 100 ms | min/max temperature 0.1 C i16/i16, temperature count u16, cell count u16 |
| `0x04` | `0x604` | `CAN_BMS_MSG_FAULTS` | 100 ms | active fault mask u32, latched fault mask u32 |
| `0x05` | `0x605` | `CAN_BMS_MSG_OUTPUTS_LIMITS` | 100 ms | output/anchor bits u8, current polarity i8, discharge/charge limits A u16/u16, segment count u8, cells/segment u8 |
| `0x06` | `0x606` | `CAN_BMS_MSG_AFE_STATUS` | 100 ms | bring-up status u8, failed step u8, configured/verified segments u8/u8, BQ79600 DEVICE_CONFIG u8, reserved u8[3] |
| `0x10+` | `0x610+` | `CAN_BMS_MSG_CELL_GROUPS` | rotating at 100 ms | four cell voltages in mV u16[4] |
| `0x40+` | `0x640+` | `CAN_BMS_MSG_TEMP_GROUPS` | rotating at 100 ms | four temperatures in 0.1 C i16[4] |
| `0xF0` | `0x6F0` | `CAN_BMS_MSG_SUMMARY_PRIMARY` | 5 s | summary frame 1 |
| `0xF1` | `0x6F1` | `CAN_BMS_MSG_SUMMARY_SECONDARY` | 5 s | summary frame 2 |

## Selecting how many messages are sent

Edit `CAN_BMS_ENABLED_MESSAGES` in the same `can_protocol.h` file. The default
enables only the two five-second summary frames:

```c
#define CAN_BMS_ENABLED_MESSAGES \
    (CAN_BMS_MSG_SUMMARY_PRIMARY | CAN_BMS_MSG_SUMMARY_SECONDARY)
```

Examples:

```c
/* Only faults */
#define CAN_BMS_ENABLED_MESSAGES CAN_BMS_MSG_FAULTS

/* Pack status plus faults */
#define CAN_BMS_ENABLED_MESSAGES \
    (CAN_BMS_MSG_PACK_STATUS | CAN_BMS_MSG_FAULTS)

/* Every available BMS message */
#define CAN_BMS_ENABLED_MESSAGES CAN_BMS_MSG_ALL

/* No BMS telemetry */
#define CAN_BMS_ENABLED_MESSAGES 0UL
```

The compiler rejects an unaligned base, a namespace outside the 11-bit CAN
range, or unknown enable bits.

## Five-second summary payloads

Summary frame 1, offset `0xF0`, DLC 8:

| Bytes | Type | Scale/unit | Meaning |
|---|---|---|---|
| 0 | u8 | none | Summary format version, currently 1 |
| 1 | u8 | enum | `ams_state_t` BMS state |
| 2-3 | u16 | 0.1% (permille) | State of charge, 0-1000 |
| 4-5 | u16 | 0.1 V | Pack voltage |
| 6-7 | i16 | 0.1 A | Pack current; positive is discharge, negative is charge |

Summary frame 2, offset `0xF1`, DLC 8:

| Bytes | Type | Scale/unit | Meaning |
|---|---|---|---|
| 0-1 | u16 | mV | Minimum active-cell voltage |
| 2-3 | u16 | mV | Maximum active-cell voltage |
| 4-5 | i16 | 0.1 C | Maximum measured cell temperature |
| 6-7 | u16 | bit mask | Active AMS fault bits 0-15 |

Pack voltage and current saturate at their encoded type limits. Until the first
valid AFE sample, measurement fields are zero and the AFE communication fault
indicates that the values are unavailable. For AFE status, status 0 and failed
step 0 indicate successful bring-up; configured and verified segment counts
must match, and the expected BQ79600 DEVICE_CONFIG value is `0x14`.
