# STM32F446 board-support contract

`board.c` is intentionally register-level and does not depend on STM32 HAL.
Complete the ICM-20948 I2C transaction routine against the exact board revision.
The BMP390/TMP117 routines are optional when `ENABLE_ENV_SENSORS` is zero.

Expected resource map:

| Function | Peripheral | DMA stream/channel | IRQ priority |
|---|---|---|---|
| IMU/environment RX | I2C1 | DMA1 S0, channel 1 | 6 |
| IMU/environment TX | I2C1 | DMA1 S7, channel 1 | 6 |
| Telemetry TX | USART2 | DMA1 S6, channel 4 | 8 |
| FreeRTOS tick | SysTick | — | 15 |

DMA interrupt priorities must be numerically equal to or greater than
`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`, because the completion ISRs use
`vTaskNotifyGiveFromISR`. The minimum build schedules a 200 Hz read. An optional
EXTI data-ready input should only release the task; never perform I2C work in
the ISR.

I2C1 TX deliberately uses Stream 7. Although Stream 6 supports both I2C1_TX
channel 1 and USART2_TX channel 4, a DMA stream cannot serve both peripherals
concurrently; assigning USART telemetry and environmental sampling to Stream 6
would create an intermittent runtime collision.

Required implementation sequence:

1. Enable FPU access before any floating-point task starts.
2. Configure HSE/PLL for 180 MHz and APB clocks within STM32F446 limits.
3. Enable DWT CYCCNT for the wrap-safe microsecond timestamp.
4. Configure GPIO alternate functions before enabling peripherals.
5. Clear every DMA flag before enabling a stream.
6. On a timeout, disable the stream, wait for EN to clear, clear flags, and
   reset/reinitialize the peripheral.
7. Check ICM-20948 WHO_AM_I (`0xEA`) before starting acquisition.
8. Restore PLL/system clock after STOP before resuming the scheduler.

The application never feeds the watchdog from an ISR or idle hook. That is
deliberate: a live interrupt stream must not disguise deadlocked tasks.
