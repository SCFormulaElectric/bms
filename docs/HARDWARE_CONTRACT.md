# Hardware commissioning contract

## Confirmed from the Custom_BMS design

- STM32F405RGTx, LQFP64, 8 MHz HSE;
- BQ79600-Q1 on SPI1: PA3 RDY, PA4 nCS, PA5 SCLK, PA6 MISO, PA7 MOSI,
  PA8 nFAULT;
- BQ79616-Q1 daughterboards, 12 routed cell channels and four routed
  thermistor channels per segment;
- PA1 and PA2 are the two external-current-sensor analog inputs;
- CAN1 on PA11/PA12 through SN65HVD230 at 500 kbit/s;
- USART1 on PA9/PA10 at J5;
- PB0 green LED;
- PB3 discharge, PB4 charge and PB6 fan low-side controls;
- PB5 CHARGE_ON, PB1 CP_CTRL, PA15 proximity and PC10 CP_DETECT;
- no external EEPROM and no SD-card interface.

## Evidence required before output commissioning

- installed current-sensor part number, which of PA1/PA2 is authoritative,
  measured zero offset, gain, safe range and sign;
- thermistor part number and validated ADC-code-to-temperature conversion;
- final number/order of segments and populated channels;
- cell manufacturer limits and approved operational thresholds;
- bench confirmation of PB3/PB4/PB6 downstream polarity, reset behavior,
  broken-wire behavior and interaction with the shutdown circuit;
- non-programmable AMS/IMD latch and manual-reset behavior;
- oscilloscope verification of the implemented BQ79600 GPIO/stack wake timing,
  5.25 MHz SPI traffic and SPI_RDY behavior on this layout;
- with `CAN_BMS_MSG_AFE_STATUS` enabled, confirmation that CAN ID
  `CAN_BMS_BASE_ID + 0x06` reports status 0, equal configured and verified
  segment counts, and bridge DEVICE_CONFIG 0x14;
- BQ79616 measurement setup, open-wire diagnostics, protector configuration
  and conversion timing verified against physical hardware;
- balancing resistor thermal characterization and proof that balancing stops
  whenever the shutdown circuit is open;
- charger model, CAN protocol, charger-presence behavior and charging shutdown
  integration;
- final CAN DBC and GUI configuration authorization policy.

Until these are complete, the AFE and external-output commissioning paths must
remain disabled.
