# Hardware acceptance checklist

All numbers below are acceptance targets until a dated capture is committed.

| Test | Method | Pass criterion |
|---|---|---|
| Sampling rate | GPIO marker at DMA half callback, 60 s | 10.240 ms block period within 0.1%; no gaps |
| Dual-channel path | PA4 wired to PA0 and PA1 | both detect 976.5625 Hz; frequency error < one FFT bin |
| FFT deadline | DWT cycle count and DSP GPIO pulse | maximum processing time < 9.0 ms |
| Spectrum output | Python monitor for 30 min | 20 results/s/channel; no unexpected sequence loss |
| Protocol faults | Python unit suite | malformed/CRC/truncated/escape cases recover |
| Timestamp wrap | run beyond 24 s and inject values around u32 wrap | no false gap at the former DWT wrap; wrap-safe subtraction |
| DMA ownership | delay DSP beyond one block in fault build | stale half-buffer rejected and flagged |
| Signal quality | disconnect, ground, then overdrive within safe limits | weak/frozen/clipping flags; valid bit cleared |
| UART saturation | worst-case escaped test data | no transmit timeout or flagged drop |
| Stack margin | `uxTaskGetStackHighWaterMark()` after stress | at least 25% free per task |
| Watchdog | compile-time DSP-stall injection | IWDG reset occurs; reset cause reported |
| Idle behavior | timing trace around `WFI` or debugger counter | CPU enters sleep between runnable work |

Record board revision, firmware commit, compiler version/options, clock
configuration, test duration, and raw CSV/logic traces. A logic analyzer shows
GPIO and UART activity, not DMA itself; GPIO markers establish the relationship
between DMA completion and task execution.

Suggested evidence files after the board arrives:

```text
evidence/run-YYYYMMDD/metadata.md
evidence/run-YYYYMMDD/telemetry.csv
evidence/run-YYYYMMDD/dma-timing.sr
evidence/run-YYYYMMDD/stack-and-wcet.md
evidence/run-YYYYMMDD/spectrum.png
```

Use `--csv` for per-frame summaries, `--spectrum-csv` for all 512 frequency
bins, and `--waveform-csv` for all 128 preview samples.
