# AMS firmware architecture

## Safety boundary

The firmware may request that the AMS shutdown-circuit power stage remain
closed. It is not the power stage itself. The hardware must be de-energized-open
and must open the shutdown circuit on MCU reset, loss of power, broken wiring,
or loss of the software request.

No CAN or USB message is safety-authoritative. Manual fault reset and contactor
activation are intentionally absent from this foundation.

## Critical cycle

The watchdog-proven cycle is:

1. acquire all AFE measurements;
2. validate completeness and communication status;
3. evaluate immediate and persistence-qualified faults;
4. latch confirmed faults;
5. update the shutdown request and fault indicator;
6. publish the watchdog heartbeat.

The watchdog is refreshed only after this whole sequence has completed.
Logging, CAN, USB, and SD-card availability are excluded from the critical
heartbeat set.

## AFE DMA pipeline

The AFE does not directly control STM32 memory. The STM32 starts each SPI/isoSPI
transaction and DMA moves bytes between the SPI data register and fixed RAM
buffers. The nonblocking pipeline is:

1. DMA-send the cell-conversion command;
2. wait the device-profile conversion interval;
3. DMA-read and decode all cell groups;
4. DMA-send the auxiliary/temperature conversion command;
5. wait the auxiliary conversion interval;
6. DMA-read and decode all temperature groups;
7. DMA-read and validate diagnostics;
8. atomically publish the completed measurement buffer.

Raw TX/RX storage and decoded measurements both use ping-pong buffers. DMA
callbacks only mark transfer completion or error and release chip select. They
do not publish measurements or make safety decisions. Every transfer has a
fixed length and timeout. The device profile must perform PEC/CRC validation
and return an error before publication when any device or register group is
invalid.

## Fault persistence

Voltage and current thresholds use a 500 ms persistence window and temperature
thresholds use a 1000 ms persistence window as the current 2026 Formula Student
baseline. A transient violation resets only its pending timer. Once a fault is
confirmed, the resulting shutdown fault is latched and is not debounced or
automatically cleared.

AFE communication failure, incomplete measurement sets, uncommissioned
topology, and internal software failures are immediate fail-closed faults.

## Commissioning gates

`AMS_TOPOLOGY_COMMISSIONED` proves that cell/segment/sensor counts and limits
match the reviewed accumulator design.

`AMS_HARDWARE_OUTPUTS_COMMISSIONED` proves that output polarity, open-wire
behavior, the non-programmable power stage, manual latch/reset, AIR feedback,
and bench fault injection have been verified.

Both gates default to zero and are deliberately unavailable through CAN, USB,
or runtime configuration.
