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

The firmware emits one complete summary and one rotating cell/temperature group
every 100 ms after a valid AFE snapshot. Incoming CAN configuration is not yet
enabled and no incoming frame can directly command a safety output.

For frame 0x506, status 0 and failed step 0 indicate success. The configured
and verified segment counts must match, and the expected BQ79600
DEVICE_CONFIG value is 0x14. Nonzero status/step values correspond to the
enums in `Core/Inc/Ams/bq79600_bridge.h`.
