# BMS CAN telemetry

Bus speed is 500 kbit/s, standard 11-bit identifiers, little-endian fields.
Definitions are provisional until captured in the team DBC.

| ID | Payload |
|---|---|
| 0x500 | state u8, rolling counter u8, SOC permille u16, sample time ms u32 |
| 0x501 | pack voltage mV i32, pack current mA i32 |
| 0x502 | min/max cell mV u16/u16, min/max indices u8/u8, SOC permille u16 |
| 0x503 | min/max temperature 0.1 C i16/i16, temperature count u16, cell count u16 |
| 0x504 | active fault mask u32, latched fault mask u32 |
| 0x505 | output/anchor bits u8, current polarity i8, discharge/charge limits A u16/u16, segment count u8, cells/segment u8 |
| 0x506 | AFE bring-up status u8, failed step u8, configured segments u8, verified segments u8, BQ79600 DEVICE_CONFIG u8, reserved u8[3] |
| 0x510+ | rotating groups of four cell voltages in mV |
| 0x580+ | rotating groups of four temperatures in 0.1 C |
| configurable base ID (default 0x5F0) | five-second BMS summary frame 1 |
| configurable base ID + 1 (default 0x5F1) | optional five-second BMS summary frame 2 |

The firmware emits one complete summary and one rotating cell/temperature group
every 100 ms after a valid AFE snapshot. Incoming CAN configuration is not yet
enabled and no incoming frame can directly command a safety output.

For frame 0x506, status 0 and failed step 0 indicate success. The configured
and verified segment counts must match, and the expected BQ79600
DEVICE_CONFIG value is 0x14. Nonzero status/step values correspond to the
enums in `Core/Inc/Ams/bq79600_bridge.h`.

## Configurable five-second summary

The summary task runs independently of the critical measurement task and puts
messages into the existing nonblocking CAN transmit queue. Configuration fields
`periodic_can_base_id` and `periodic_can_frame_count` are persisted with the
other BMS settings. Frame count accepts only 1 or 2. All multibyte fields are
little-endian.

Summary frame 1, base ID, DLC 8:

| Bytes | Type | Scale/unit | Meaning |
|---|---|---|---|
| 0 | u8 | none | Summary format version, currently 1 |
| 1 | u8 | enum | `ams_state_t` BMS state |
| 2-3 | u16 | 0.1% (permille) | State of charge, 0-1000 |
| 4-5 | u16 | 0.1 V | Pack voltage; firmware converts mV by dividing by 100 |
| 6-7 | i16 | 0.1 A | Pack current; positive is discharge, negative is charge |

Summary frame 2, base ID + 1, DLC 8, emitted only when frame count is 2:

| Bytes | Type | Scale/unit | Meaning |
|---|---|---|---|
| 0-1 | u16 | mV | Minimum active-cell voltage |
| 2-3 | u16 | mV | Maximum active-cell voltage |
| 4-5 | i16 | 0.1 C | Maximum measured cell temperature |
| 6-7 | u16 | bit mask | Active AMS fault bits 0-15 |

Pack voltage and current saturate at their encoded type limits. Until the first
valid AFE sample, measurement fields are zero and the active AFE communication
fault indicates that the values are unavailable.
