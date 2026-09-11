# FreeRTOS Dual-Channel Real-Time Spectrum Analyzer

Portfolio firmware for the STM32F446RE. The design samples two analog channels
simultaneously at 100 kS/s per channel, processes 1024-sample blocks, and emits
RMS, peak, dominant-frequency, waveform-preview, and spectrum data at 20 Hz.

The minimum hardware is one NUCLEO-F446RE, its Mini-USB data cable, and two
male-to-male jumper wires. The STM32 DAC generates a coherent 976.5625 Hz
self-test tone; wiring PA4 to PA0 and PA1 closes the complete DAC-to-ADC loop.
No sensor module, soldering, external programmer, or USB-to-UART adapter is
required. A logic analyzer is useful evidence but is not required to run it.

## Honest completion status

| Status | Scope |
|---|---|
| Host verified | CRC-16, framing, recovery, spectrum reassembly, bandwidth bound, and 14 Python/fault-injection tests; portable FFT/C tests pass Cortex-M4 strict compile checks but were not executed because this PC has no native C toolchain |
| Implemented; target build pending | FreeRTOS queues/tasks, dual-ADC DMA adapter, CMSIS-DSP Q15 backend, DAC DMA self-test, USART2 TX DMA, task health voting, IWDG policy, and WFI idle |
| Requires the physical board | CubeMX-generated HAL project integration, flash/run, 100 kS/s timing, CMSIS-DSP WCET, UART endurance, stack high-water marks, watchdog reset, and captured evidence |

No measured hardware number is claimed before it is measured. Files under
`platform/stm32f446/` are integration-ready adapters, not proof that the target
binary has already run.

## Architecture

- ADC1 and ADC2 use dual regular simultaneous mode, triggered by TIM2 at
  100 kHz. DMA stores packed 12-bit samples in a two-half circular buffer.
- The acquisition task is released by DMA half/full-complete notifications and
  checks timestamp/generation continuity.
- The DSP task applies a Hann window and 1024-point FFT independently to both
  channels, calculates RMS/peak/dominant frequency, and overwrites a one-entry
  latest-result queue.
- The telemetry task compresses Q15 magnitudes to 8-bit values and sends eight
  bounded packets per result using USART2 TX DMA through the board's ST-LINK
  virtual COM port at 921600 baud.
- The watchdog task feeds IWDG only after acquisition, DSP, and telemetry have
  all reported progress within the voting window.
- Idle uses `WFI`; deeper STOP-mode claims are deliberately outside this
  continuous 100 kS/s instrument.

See [architecture](docs/architecture.md), [protocol](docs/protocol.md), and the
[timing budget](docs/timing_budget.md).

## Repository map

```text
app/                 FreeRTOS application tasks
core/                Portable FFT reference, health checks, protocol
include/             Shared interfaces and data structures
platform/stm32f446/  HAL/DMA adapter and CMSIS-DSP target backend
tools/               Python decoder, CSV logger, live plotter
tests/               C behavior tests and Python fault-injection tests
docs/                Wiring, architecture, timing, and validation
```

## Verify without hardware

```powershell
python -m unittest discover -s tests -v
python tools\spectrum_monitor.py --self-test
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
```

The PowerShell script uses the ARM GCC shipped under `C:\ST`, compiles the
portable C modules and C test source with warnings as errors, and runs the
Python suite. This is a compile check, not execution of the ARM objects. If a
native C compiler and CMake are installed, the portable executable can
additionally be built and run with CTest.

For live serial input and plotting, install the two host-only packages:

```powershell
python -m pip install -r requirements.txt
```

## Build and run on the board

Generate the STM32CubeIDE project using the exact settings in
[platform/stm32f446/README.md](platform/stm32f446/README.md), add these sources,
and compile the target backend instead of the portable FFT backend. Connect:

```text
PA4 / A2 (DAC output) -> PA0 / A0 (ADC channel 1)
PA4 / A2 (DAC output) -> PA1 / A1 (ADC channel 2)
```

Open the ST-LINK virtual COM port at 921600 8-N-1, then run:

```powershell
python tools\spectrum_monitor.py --port COM5 --baud 921600 --plot --csv capture.csv
```

The expected built-in tone is FFT bin 10:
`100000 * 10 / 1024 = 976.5625 Hz`. Replace `COM5` with the enumerated port.

## Resume wording

Use these bullets only after the hardware acceptance sheet passes:

- Developed a dual-channel real-time spectrum analyzer on STM32F446 using
  timer-triggered simultaneous ADC sampling, DMA ping-pong buffering, and a
  CMSIS-DSP 1024-point Q15 FFT at 100 kS/s per channel.
- Designed a priority-scheduled FreeRTOS acquisition/DSP/telemetry pipeline and
  streamed bounded CRC-protected binary frames over USART DMA at 20 Hz.
- Validated frequency accuracy, end-to-end latency, task stack margin, watchdog
  recovery, and protocol fault handling using DAC loopback, GPIO timing traces,
  and automated Python tests.

Before hardware validation, change “Developed/Validated” to
“Implemented the software architecture for” and “Host-tested.”

## License

MIT.
