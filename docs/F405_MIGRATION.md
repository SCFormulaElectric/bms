# STM32F405 motherboard migration

## Design sources

The maintained AMS implementation is retargeted to the STM32F405RGT6 specified
by the 2026-2027 USC BMS project. The current 2026-2027 Cube project contains
only a PB0 bring-up output, so the provisional peripheral map is recovered from
the repository's 2025-2026 Custom_BMS project for the same MCU and package.

## Provisional motherboard map

- PA1: ADC1_IN1, analog current/voltage sensing
- PA2: ADC1_IN2, analog current/voltage sensing
- PA3: TI bridge SPI-ready input
- PA4: TI bridge active-low chip select
- PA5/PA6/PA7: SPI1 SCK/MISO/MOSI
- PA8: TI bridge active-low fault input
- PA11/PA12: CAN1 RX/TX
- PB0: provisional active-high relay/SDC request
- PH0/PH1: 8 MHz external oscillator

ADC1 DMA remains on DMA2 Stream0. SPI1 RX/TX use DMA2 Stream2 and Stream3,
respectively, avoiding the Stream0 collision present in the historical project.
USB is not initialized because its PA11/PA12 data pins conflict with CAN1.

## Preserved implementation

The FreeRTOS task structure, watchdog-proven critical cycle, fault persistence
and latching, diagnostic CAN transmission, host tests, and nonblocking AFE DMA
pipeline are preserved. The hardware output and topology commissioning gates
remain zero.

## Requirements still to implement

The 2026-2027 requirements add EEPROM first-fault retention, calibrated current
and pack-voltage sensing, SOC/SOH, temperature aggregation, UART, ELCON charger
control, and final TI daisy-chain framing. These require hardware identifiers,
scaling, pin assignments, CAN definitions, and validation evidence before they
can safely be enabled.
