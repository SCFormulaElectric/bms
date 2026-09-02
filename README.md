# Formula SAE Accumulator Management System Firmware

Custom STM32 firmware for a Formula SAE / Formula Student electric vehicle
Accumulator Management System (AMS).

## Current status

The repository contains a buildable, fail-closed AMS foundation. It is not
ready to energize a tractive system.

- Target MCU: STM32F407VET6
- Scheduler: FreeRTOS
- Vehicle interface: CAN 2.0 at 500 kbit/s
- AFE: not selected or commissioned
- Accumulator topology: placeholder only
- Shutdown output: compile-time locked open

At startup the placeholder AFE reports `AMS_AFE_NOT_COMMISSIONED`. The critical
cycle latches an AFE fault and keeps the active-high shutdown request low.

The acquisition layer is nonblocking and DMA-backed. SPI1 uses DMA2 Stream2
for RX and DMA2 Stream3 for TX. Fixed ping-pong raw buffers feed separate
decoded measurement ping-pong buffers; only complete, profile-validated samples
are published to the safety logic.

## Source layout

- `Core/Inc/Ams` and `Core/Src/Ams`: hardware-independent AMS types, AFE
  interface, and fault/state logic
- `Core/Src/Tasks/Critical/ams_cycle_task.c`: measurement-to-shutdown critical
  cycle and watchdog heartbeat
- `Core/Src/Tasks/CAN`: non-safety-authoritative diagnostic CAN transmission
- `Core/Src/Tasks/DAQ`: optional logging
- `tests/host`: hardware-independent unit tests
- `docs/ARCHITECTURE.md`: safety boundaries and migration decisions
- `docs/HARDWARE_CONTRACT.md`: information still required from the hardware team

## Build

From PowerShell:

```powershell
.\tools\build.ps1 -Configuration Debug
.\tools\build.ps1 -Configuration Release
```

Do not set either commissioning gate in `Core/Inc/Ams/ams_config.h` until the
corresponding hardware review and test evidence exist.
