# STM32F446 board-support contract

`board.c` is intentionally register-level and does not depend on STM32 HAL.
Complete the four sensor conversion routines against the exact breakout boards
you own; compensation coefficients differ by sensor revision.

Expected resource map:

| Function | Peripheral | DMA stream/channel | IRQ priority |
|---|---|---|---|
| IMU RX/TX | SPI1 | DMA2 S0/S3, channel 3 | 6 |
| Barometer/temp RX | I2C1 | DMA1 S0, channel 1 | 7 |
| Barometer/temp TX | I2C1 | DMA1 S7, channel 1 | 7 |
| Telemetry TX | USART2 | DMA1 S6, channel 4 | 8 |
| FreeRTOS tick | SysTick | — | 15 |

DMA interrupt priorities must be numerically equal to or greater than
`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`, because the completion ISRs use
`vTaskNotifyGiveFromISR`. EXTI data-ready can be priority 5 and should only
release the IMU task; do not perform SPI work in the ISR.

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
7. Restore PLL/system clock after STOP before resuming the scheduler.

The application never feeds the watchdog from an ISR or idle hook. That is
deliberate: a live interrupt stream must not disguise deadlocked tasks.
