# Firmware architecture

## Data flow and scheduling

The 200 Hz IMU task is the highest-rate producer. It starts an I2C DMA register
burst and blocks on a direct notification. The fusion task consumes calibrated
samples and publishes at 100 Hz. Telemetry decimates fused states to 50 Hz and
owns UART DMA. The environmental task is compiled out in the no-solder minimum
build and can be enabled later without changing the fusion or telemetry APIs.

| Priority | Task | Period / deadline | WCET target | Stack |
|---:|---|---|---:|---:|
| 6 | watchdog | 250 ms | 50 us | 256 words |
| 5 | IMU acquisition | 5 ms | 500 us | 384 words |
| 4 | fusion | event-driven / 10 ms output | 180 us | 640 words |
| 3 | environment (optional) | 20 ms | 500 us | 384 words |
| 2 | telemetry | 20 ms | 200 us CPU | 384 words |
| 1 | power policy | 100 ms | 50 us | 256 words |

Detailed utilization, stack headroom criteria, and the hardware measurement
procedure are in [timing_budget.md](timing_budget.md).

All queues are bounded and statically allocated. When a producer outruns a
consumer, the oldest sample is discarded and a sticky status bit records the
event. This prioritizes freshness, which is the correct failure mode for a
real-time estimator.

## Filter model

Roll and pitch each use a two-state Kalman model: angle and gyro bias. The
prediction integrates angular rate; accelerometer tilt supplies the observation.
During linear acceleration, observation noise is inflated by 1000x rather than
allowing apparent gravity to jerk the attitude estimate. Without a magnetometer,
yaw is explicitly dead-reckoned and will drift.

Pressure is converted using the standard-atmosphere approximation and filtered
with a scalar Kalman update. Sea-level pressure is configurable; incorrect local
pressure produces an altitude offset, not instability.

## Recovery model

- Three consecutive sensor transaction failures reset and reinitialize I2C1.
- DMA waits have deadlines; timeout recovery disables the stream and clears all
  flags before reuse.
- Each critical task votes once per watchdog window. Only a complete vote set
  feeds the independent watchdog.
- Task liveness and sensor-data health are independent: a running task still
  votes even when a plausibility check rejects its sample. Rejected samples do
  not enter the fusion queues and set a telemetry fault bit.
- Reset-cause flags are captured before clearing and reported in the first
  telemetry packets.
- Protocol errors are receiver-local and resynchronize at the next delimiter.

## Power states

NORMAL runs every sensor and clocks the core at full rate. IDLE uses `WFI` and
tickless idle while DMA/EXTI/timers remain wake sources. STOP disables the PLL
and high-rate sampling; RTC or an external interrupt wakes the board, restores
clocks, reinitializes time bases, and restarts sensors before valid bits return.

STOP is only appropriate for explicitly quiescent periods. Entering it between
200 Hz samples would cost more wake energy than it saves.
