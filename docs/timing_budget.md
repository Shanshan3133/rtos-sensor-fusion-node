# Real-time timing and stack budget

All targets below are design limits, not measured claims. Hardware values stay
marked `TBD` until captured on the NUCLEO-F446RE release build. Cortex-M4
`StackType_t` is 32 bits, so the byte allocations are four times the values
passed to `xTaskCreate`.

| Task | Priority | Activation/deadline | WCET target | Budgeted CPU | Stack allocation | Measured WCET | Minimum free stack |
|---|---:|---:|---:|---:|---:|---:|---:|
| watchdog | 6 | 250 ms | 50 us | 0.02% | 256 words / 1024 B | TBD | TBD |
| IMU acquisition | 5 | 5 ms | 500 us | 10.0% | 384 words / 1536 B | TBD | TBD |
| fusion | 4 | 5 ms input; 10 ms publish | 180 us | 3.6% | 640 words / 2560 B | TBD | TBD |
| environment (optional) | 3 | 20 ms | 500 us | 2.5% | 384 words / 1536 B | TBD | TBD |
| telemetry | 2 | 20 ms | 200 us | 1.0% | 384 words / 1536 B | TBD | TBD |
| power policy | 1 | 100 ms | 50 us | 0.05% | 256 words / 1024 B | TBD | TBD |

The default four-task acquisition/fusion/telemetry/power path plus watchdog has
a 14.67% static CPU budget. Enabling the environmental task raises it to
17.17%. This leaves ample margin for interrupt service, scheduler overhead, DMA
completion jitter, and future commands.

## Measurement procedure

1. Build with optimization matching the release configuration and enable DWT
   `CYCCNT` plus `configGENERATE_RUN_TIME_STATS`.
2. Toggle one spare GPIO around each measured active section. Use the logic
   analyzer maximum pulse width as WCET; do not count time blocked on a task
   notification as CPU execution.
3. Run at least 30 minutes with maximum UART traffic and sensor rates. Inject
   CRC errors and I2C timeouts during the run.
4. Record `uxTaskGetStackHighWaterMark()` after the stress run. Require at
   least 25% of each allocation free; increase a stack if it falls below that
   threshold.
5. Fail acceptance if any measured WCET exceeds its target, a 5 ms IMU
   deadline is missed, or total measured utilization exceeds 70%.

Keep the raw trace, build commit, compiler flags, and table of measured values
together. Numbers without that evidence are estimates, not validation results.
