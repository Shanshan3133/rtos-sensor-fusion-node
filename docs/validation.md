# Hardware validation checklist

Record firmware commit, board revision, sensor lot IDs, compiler version, and
ambient conditions with every run.

| Test | Method | Pass criterion |
|---|---|---|
| IMU cadence | data-ready and SPI CS on logic analyzer, 10 s | 1000 Hz ±0.1%; no missed DRDY |
| DMA behavior | GPIO markers around start/completion | CPU is not polling; completion <120 us |
| Fusion deadline | DWT cycle instrumentation, 10 min | p99.9 <180 us; zero 5 ms misses |
| UART integrity | capture 1 hour, run decoder | zero CRC errors; zero sequence gaps |
| Static attitude | six orthogonal faces | roll/pitch error <1.0 degree RMS |
| Gyro bias | stationary, 10 min | post-cal mean <0.01 rad/s each axis |
| Altitude noise | stationary, 10 min | filtered standard deviation <0.5 m |
| Thermal drift | 10–50 C chamber or controlled ramp | plot bias vs temperature; no resets |
| I2C fault | momentarily hold SDA low | bounded timeout; recovery status set |
| Task deadlock | compile-time fault injection | IWDG resets in 1.0 s ±LSI tolerance |
| Brownout | controlled supply ramp | clean reset; no corrupt calibration |
| Power | series ammeter in each state | log NORMAL/IDLE/STOP current |

Before claiming numbers, measure them. Targets above are acceptance goals, not
results. Preserve raw captures and scripts so another engineer can reproduce
every chart.
