# RTOS IMU Sensor Fusion Node

A no-solder reference design for the STM32F446. It is designed to sample a
Qwiic ICM-20948 IMU over I2C DMA, estimate attitude, and emit framed binary
telemetry over a DMA-backed UART. BMP390 and TMP117 support remains as an
optional second phase; the default firmware does not require either sensor.

The repository deliberately separates the portable signal-processing and wire
protocol code from the board support package. This makes the difficult parts
testable on a workstation while keeping timing, DMA, watchdog, and power
behavior visible in the firmware.

## Verification status

This repository does **not** claim completed STM32 hardware validation yet.
The boundary is explicit:

| Status | Scope |
|---|---|
| Host verified | ICM-20948 raw-data decoding, calibration and Kalman math, telemetry framing/CRC/escaping, receiver resynchronization, and Python fault-injection tests |
| Implemented; target runtime pending | FreeRTOS task/queue orchestration, health voting, watchdog policy, UART DMA path, and power-state interfaces |
| Hardware validation pending | `platform_imu_read_dma()`, complete I2C1/DMA initialization and ISR binding, sustained 200 Hz acquisition, logic-analyzer waveforms, measured WCET/stack headroom, watchdog reset timing, and current consumption |

`platform_imu_read_dma()` intentionally remains a fail-closed board-support
stub until the exact NUCLEO and sensor hardware is connected. All timing values
marked as targets or `TBD` are acceptance criteria, not measured results.

## Engineering trade-off

The initial concept used a bare ICM-42688-P breakout over SPI. To reduce
assembly and voltage-level risk during portfolio validation, the reference
hardware was changed to a SparkFun ICM-20948 Qwiic breakout using 400 kHz I2C
DMA at 200 Hz. This preserves the RTOS architecture, asynchronous DMA flow,
filtering, fault handling, and protocol work while making the first hardware
milestone solder-free. SPI and the environmental sensors remain possible later
extensions rather than unverified claims in the minimum build.

## No-solder reference hardware

| Part | Role | Bus / rate |
|---|---|---|
| NUCLEO-F446RE | MCU, Cortex-M4F at 180 MHz | — |
| SparkFun ICM-20948 Qwiic (SEN-15335) | 9-axis IMU; accel + gyro used first | I2C1 DMA, 200 Hz |
| Qwiic-to-male jumper cable | Direct connection to NUCLEO headers | — |
| ST-LINK VCP | telemetry and logs | USART2, 921600 8-N-1 |
| 8-channel logic analyzer | I2C/UART evidence | — |

No header soldering, pull-up resistor, level shifter, external ST-LINK, or
USB-to-UART adapter is required for this build. The SparkFun board provides
regulation, level shifting, and Qwiic connectivity. A breadboard is optional
for making logic-analyzer tap points. A BMP390 and TMP117 can later be
daisy-chained over Qwiic without changing the core design.

See [docs/hardware.md](docs/hardware.md) for the wiring and [docs/architecture.md](docs/architecture.md)
for task priorities, data flow, timing budget, recovery, and power states.

## What is implemented

- FreeRTOS task layout with rate-monotonic priorities and bounded queues
- I2C and UART transfer contracts designed for DMA completion notifications
- Two-state Kalman filters for roll and pitch (angle + gyro bias)
- Optional scalar Kalman filter for a future barometric-altitude extension
- Stationary IMU calibration with variance/rejection checks
- Versioned binary UART protocol with sequence numbers, timestamps, status bits,
  saturation-safe fixed-point fields, and CRC-16/CCITT-FALSE
- Independent-watchdog health voting (all critical tasks must make progress)
- NORMAL, IDLE, and STOP power policy with explicit wake behavior
- Firmware/Python telemetry decoders with bounded resynchronization, detailed
  error counters, and fault-injection tests

## Repository map

```text
app/                 FreeRTOS tasks and application orchestration
core/                Portable fusion, calibration, CRC, and packet encoding
drivers/             Tested raw sensor conversion helpers
platform/stm32f446/  Register-level board/DMA/power implementation contract
include/             Public interfaces
tools/               Host decoder / CSV logger
tests/               Portable behavior and protocol tests
docs/                Architecture, hardware, validation plan
```

## Quick verification (no board required)

Python 3.10+ is sufficient:

```powershell
python -m unittest discover -s tests -v
python tools/telemetry.py --self-test
```

On Windows with STM32CubeIDE installed, run the combined verification script:

```powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
```

The ten host checks validate CRC, escaping, fixed-point decoding, truncated and
oversized frames, illegal/dangling escapes, resynchronization, and sequence-loss
accounting.

The C tests also cover stationary fusion, first-sample barometer behavior,
calibration motion rejection, yaw integration, and raw sensor conversion. They
run automatically once a C compiler is available through CubeIDE or CMake.

## Firmware integration

This source tree is intended to be added to an STM32CubeIDE/CMake STM32F446
project containing CMSIS, the STM32F4 device headers/startup code, and FreeRTOS.
Define `SENSOR_NODE_STM32F446`, enable the FPU, and add the repository `include/`
directory. The only board-specific functions that must be connected are listed
in [platform/stm32f446/README.md](platform/stm32f446/README.md).

Suggested compiler flags:

```text
-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
-ffunction-sections -fdata-sections -Wall -Wextra -Werror
```

Start with the debugger build and `configASSERT` enabled. Do not enable STOP
mode until DMA transfers and wake-clock restoration have passed the validation
checklist.

## Telemetry protocol

Frames use SLIP-style byte stuffing. On the wire:

```text
0x7E | escaped(header + payload + crc16) | 0x7E
```

`0x7E` and `0x7D` inside the frame are escaped as `0x7D` followed by the byte
XOR `0x20`. All multibyte values are little-endian. Packet type `0x01` is the
44-byte telemetry payload described in [docs/protocol.md](docs/protocol.md).

## Evidence to capture on hardware

The code is only half the project. For a strong portfolio demonstration, attach:

1. Logic-analyzer traces showing the 200 Hz I2C burst and DMA-complete ISR.
2. Runtime-stat screenshots showing deadlines and CPU load under UART pressure.
3. A six-position IMU calibration report and stationary/no-motion Allan-style plot.
4. Watchdog recovery video with the fault-injection pin held active.
5. Current measurements in NORMAL, IDLE, and STOP states.

Use [docs/validation.md](docs/validation.md) as the acceptance sheet.

## License

MIT. The no-solder build targets ICM-20948 WHO_AM_I `0xEA` at I2C address
`0x69`. Sensor register definitions must still be checked against the exact
silicon revision received.
