# Hardware contract required for implementation

The following items must be supplied before the placeholder driver and pin map
can be commissioned:

- selected AFE and isoSPI/SPI transceiver part numbers;
- confirmed SPI mode, maximum clock, chip-select pin, and wake timing;
- number and order of devices in the daisy chain;
- cells and temperature channels wired to every device;
- PEC/CRC behavior, conversion modes, and maximum transaction timing;
- thermistor part number, divider topology, reference voltage, and calibration;
- pack-current sensor part number, polarity, range, bandwidth, and interface;
- pack-side and vehicle-side voltage measurement interfaces;
- AIR, precharge, discharge, and welded-contactor feedback signals;
- AMS SDC output pin, polarity, driver circuit, and open-wire behavior;
- non-programmable AMS/IMD latch and manual-reset circuit behavior;
- charger presence, charger-enable, and charging-SDC interfaces;
- balancing resistor value, thermal limits, and allowable operating modes;
- final cell data-sheet voltage, current, and temperature limits;
- competition/rulebook jurisdiction and approved 2026/2027 revision.

Until these are reviewed, the AFE reports not commissioned and the SDC request
cannot be asserted.
