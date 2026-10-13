# BMS firmware architecture

## Data ownership

Cell voltages, temperatures, current, extrema, SOC and fault state live in RAM.
They are published over CAN for the vehicle logger and GUI; they are not
continuously copied to flash. Internal flash stores only validated configuration
records and occasional SOC checkpoints.

The BQ79600 does not autonomously write STM32 RAM. The STM32 initiates SPI
transactions. After bring-up, `bq79600_configure_measurements` sets every
BQ79616 to `cells_per_segment` active cells (cells on VC0 up, unused inputs
tied per the TI datasheet), GPIO1..`temperatures_per_segment` as ADC-only
absolute-voltage inputs, TSREF on, a 26 Hz cell low-pass filter and continuous
main-ADC conversion, then reads ACTIVE_CELL back from every device.

At runtime the critical task reads the latest VCELL and GPIO blocks without
blocking. A BQ79600 read is three phases, not one SPI transfer: send the
command once SPI_RDY is high, wait for SPI_RDY to show the stack response is
buffered, then clock the response out with dummy bytes in chunks of at most
128 bytes, waiting for SPI_RDY before each chunk. The HAL-free, host-tested
`bq79600_reader` sequences those phases with timeouts and decodes frames with
`bq79616_measurement` (CRC, register, one frame per device). `ams_afe` is the
hardware glue: it samples SPI_RDY, frames each transfer with nCS and runs it
as SPI1 DMA. The task polls it every `AMS_AFE_SERVICE_POLL_MS` while a sample
is in flight. Any failure sends a one-byte communication clear and the next
sample starts fresh.

GPIO inputs are reported as millivolts, not temperatures. With no temperature
channels the controller still evaluates cell limits but latches
`AMS_FAULT_NOT_COMMISSIONED`, so outputs cannot be enabled without validated
temperature monitoring.

Before DMA or the scheduler starts, firmware loads the persisted topology and
runs the complete forward-direction bring-up. The board driver temporarily
changes PA7 from SPI1_MOSI to a GPIO and emits two wake pulses: nCS setup for
2 us, MOSI low for 2.75 ms, and nCS hold for 2 us. It restores PA7 to SPI,
wakes the daisy chain, synchronizes both communication DLL directions,
auto-addresses the configured BQ79616 devices, marks the top device, then reads
back every address and the BQ79600 DEVICE_CONFIG register with CRC validation.

Any transport, CRC, addressing or identity failure records an exact status and
failed step, latches the AFE communication fault, and leaves safety outputs off.
When `CAN_BMS_MSG_AFE_STATUS` is enabled, CAN offset `base + 0x06` reports the
status, step, configured segment count, verified segment count and bridge
DEVICE_CONFIG value for bring-up troubleshooting.

## Critical cycle

The watchdog-proven path acquires and validates every configured channel,
applies current calibration/polarity, updates SOC, evaluates persistence-qualified
limits, latches faults, updates charge/discharge/fan outputs, and then publishes
the watchdog heartbeat. CAN transmission is diagnostic and cannot hold up this
path.

A separate low-priority CAN configuration task accepts only the dedicated VCU
telemetry command ID. It stages, validates, commits, reads back, and optionally
persists summary layouts. This task cannot write AMS limits, clear faults, or
drive charge/discharge outputs, and it is excluded from the watchdog-proven
safety path.

Positive pack current means discharge; negative means charge. The polarity
configuration is applied before coulomb counting and current-limit checks.

Any active cell at or below 3.000 V immediately anchors usable SOC to 0% and
blocks discharge. The configurable undervoltage fault still uses its configured
persistence interval. Rested-voltage drift correction uses five monotonic,
configurable voltage/SOC points after current remains below the rest threshold.

## Safety outputs

PB3, PB4 and PB6 drive low-side stages for the external active-low discharge,
charge and fan control nets. MCU reset drives the gates low (MOSFETs off).
Firmware requests are separate and default off. PB0 is only the green status
LED.

The firmware output gate is compile-time-only and defaults off. CAN is never
safety-authoritative. Faults latch; a manual reset policy must be implemented
and validated with the non-programmable shutdown circuitry.

## Internal flash

STM32 sector 11 is an append-only record journal. Each record has a magic
number, schema version, sequence, complete configuration, SOC checkpoint and
CRC-32. Invalid/torn records are ignored. Erase/program calls are rejected
while charge or discharge outputs are active. Saving must run from a maintenance
or shutdown context, not the critical cycle.

If the sector fills, it is erased before the next record. A power interruption
during that erase can lose the saved configuration, so the application must
fall back safely and require configuration verification. A two-sector
transactional store can be added if retaining configuration through arbitrary
power loss is required.
