# Hardware and wiring

Reference board: NUCLEO-F446RE, 3.3 V logic. Confirm that every breakout board
is also operating at 3.3 V and does not contain pull-ups to 5 V.

| Signal | STM32 pin | Destination |
|---|---|---|
| SPI1_SCK | PA5 | ICM-42688 SCLK |
| SPI1_MISO | PA6 | ICM-42688 SDO |
| SPI1_MOSI | PA7 | ICM-42688 SDI |
| IMU_CS | PB6 GPIO | ICM-42688 CS |
| IMU_INT1 | PC7 EXTI | ICM-42688 INT1/data-ready |
| I2C1_SCL | PB8 | BMP390 + TMP117 SCL |
| I2C1_SDA | PB9 | BMP390 + TMP117 SDA |
| USART2_TX | PA2 | ST-LINK VCP RX |
| GND / 3V3 | GND / 3V3 | all sensors |

Use a 100 nF ceramic capacitor at each sensor and 4.7 kOhm pull-ups on SDA/SCL
if the breakout boards do not already provide them. Keep the SPI wiring short.

Expected addresses are BMP390 `0x76` (SDO low) and TMP117 `0x48` (ADD0 low).
Verify WHO_AM_I/chip IDs at startup and refuse to publish the associated valid
bit if they do not match.

The NUCLEO virtual COM port is commonly wired to USART2. Check solder bridges
for the exact board revision before attaching another UART adapter.
