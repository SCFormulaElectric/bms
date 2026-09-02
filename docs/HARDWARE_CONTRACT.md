# Hardware contract required for implementation

The STM32F405RG target and provisional motherboard pin map are established, but
the following items must be supplied before the driver and outputs can be
commissioned:

- final TI monitor and bridge part numbers (the historical code suggests a
  BQ79600-family bridge, but this is not yet a commissioned selection);
- confirmed SPI mode, maximum clock, chip-select pin, and wake timing;
- number and order of devices in the daisy chain;
- cells and temperature channels wired to every device;
- PEC/CRC behavior, conversion modes, and maximum transaction timing;
- thermistor part number, divider topology, reference voltage, and calibration;
- pack-current sensor part number, polarity, range, bandwidth, and interface;
- pack-side and vehicle-side voltage measurement interfaces;
- AIR, precharge, discharge, and welded-contactor feedback signals;
- confirmation that PB0 is the relay/SDC request, including polarity, driver
  circuit, reset state, and open-wire behavior;
- confirmation that the motherboard external oscillator is 8 MHz;
- EEPROM part number, bus, address, endurance, and fault-record format;
- UART pins and electrical interface;
- ELCON charger model, CAN identifiers, command scaling, and timeout behavior;
- non-programmable AMS/IMD latch and manual-reset circuit behavior;
- charger presence, charger-enable, and charging-SDC interfaces;
- balancing resistor value, thermal limits, and allowable operating modes;
- final cell data-sheet voltage, current, and temperature limits;
- competition/rulebook jurisdiction and approved 2026/2027 revision.

Until these are reviewed, the AFE reports not commissioned and the SDC request
cannot be asserted.
