# Hardware and wiring

## No-solder minimum build

Use a NUCLEO-F446RE, SparkFun ICM-20948 Qwiic breakout (`SEN-15335`), and one
Qwiic-to-male jumper cable. The male ends plug directly into the NUCLEO female
headers; no loose pin header or breadboard contact is used.

| Signal | STM32 pin | Destination |
|---|---|---|
| I2C1_SCL | PB8 | Qwiic yellow / ICM-20948 SCL |
| I2C1_SDA | PB9 | Qwiic blue / ICM-20948 SDA |
| USART2_TX | PA2 | ST-LINK VCP RX |
| GND | GND | Qwiic black |
| 3V3 | 3V3 | Qwiic red |

The SparkFun breakout includes regulation and logic-level translation. Do not
add the generic level-shifter assortment or external I2C pull-ups. Keep the
Qwiic cable short and configure I2C1 for 400 kHz.

The default ICM-20948 address is `0x69`; WHO_AM_I must be `0xEA`. Verify both at
startup and refuse to publish `STATUS_IMU_VALID` if the ID does not match.

## Optional environmental extension

Set `ENABLE_ENV_SENSORS` to `1`, then daisy-chain Qwiic BMP390 (`0x77`) and
TMP117 (`0x48`) breakouts. Their addresses do not conflict. This phase is not
required for the minimum resume-ready hardware validation.

The NUCLEO virtual COM port is commonly wired to USART2. Check solder bridges
for the exact board revision before attaching another UART adapter.
