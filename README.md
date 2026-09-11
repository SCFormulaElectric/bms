# USC Formula SAE Custom BMS Firmware

STM32F405RG firmware targeting the USC Custom_BMS motherboard and BQ79616
daughterboards.

## Implemented foundation

- confirmed motherboard pin map and fail-safe GPIO reset states;
- CAN 2.0 telemetry at 500 kbit/s;
- runtime pack configuration for 1-8 segments, 6-12 cells and 1-4
  temperatures per segment;
- configurable voltage, temperature, charge-current and discharge-current
  limits, capacity, current calibration and current polarity;
- RAM-resident measurements and battery state;
- coulomb-counted SOC, five configurable rested-voltage drift points, full
  anchor, and an unconditional 0% anchor when any active cell is at or below
  3.000 V;
- separate charge, discharge and fan requests;
- latched fault logic and first-fault capture;
- CRC-checked, versioned internal-flash records for configuration and SOC;
- complete early-boot BQ79600/BQ79616 forward bring-up: dual GPIO wake,
  stack wake, write/read DLL synchronization, auto-addressing, top-of-stack
  setup, CRC-checked address verification and bridge identity verification;
- one configurable `0xNXX` CAN namespace with independent enable bits for
  every detailed, rotating and five-second summary message;
- TI-reference-tested BQ796xx command frame and CRC codec;
- nonblocking SPI/DMA acquisition framework and host tests.

## Safety status

This firmware is not ready to energize the tractive system. The compile-time
hardware-output gate remains disabled. BQ79616 measurement/protection register
configuration and channel decoding are not yet connected to the acquisition
framework, so the firmware intentionally reports an AFE communication fault
and keeps charge/discharge outputs deasserted even after successful bring-up.

Do not enable AMS_HARDWARE_OUTPUTS_COMMISSIONED until the items in
docs/HARDWARE_CONTRACT.md have bench-test evidence.

## Build

From PowerShell, run tools/build.ps1 for Debug or Release.

The final 128 KiB flash sector is excluded from the application image and
reserved for the configuration/SOC journal.

## Configuration required before vehicle testing

The values in `Core/Src/Ams/ams_config.c` are safe software defaults, not an
approved accumulator configuration. Confirm and update the segment/channel
counts, cell voltage and temperature limits, charge/discharge current limits,
pack capacity, installed current-sensor channel/polarity/zero/gain, persistence
times, measurement timeout, rested-current threshold, rest time, initial SOC
policy and all five voltage/SOC drift points before vehicle testing. The
3.000 V empty-SOC anchor is a project requirement; the other default drift
points are placeholders and must be replaced with the selected cell's tested
rested OCV curve.

For CAN output, edit only `CAN_BMS_BASE_ID` and
`CAN_BMS_ENABLED_MESSAGES` in `Core/Inc/Peripherals/can_protocol.h`. The base
selects one aligned `0xNXX` namespace and every message uses a documented
subitem offset. The enable mask permits zero, one, several or all message
types. The default sends only the two five-second summary frames. Select a
namespace that does not collide with the vehicle DBC and see
`docs/CAN_PROTOCOL.md` for every ID and byte definition.

Removing the old persisted CAN-ID fields advances the flash-record schema to
version 3. Older settings are rejected and defaults are used until a version-3
record is saved.
