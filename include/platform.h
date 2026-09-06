#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sensor_types.h"

typedef enum {
    PLATFORM_POWER_NORMAL,
    PLATFORM_POWER_IDLE,
    PLATFORM_POWER_STOP
} platform_power_mode_t;

void platform_init(void);
uint32_t platform_time_us(void);
uint32_t platform_reset_cause(void);

/* Calls block the current task on a DMA-complete notification, never by polling. */
bool platform_imu_read_dma(imu_sample_t *sample, uint32_t timeout_ms);
bool platform_baro_read_dma(baro_sample_t *sample, uint32_t timeout_ms);
bool platform_temp_read_dma(temp_sample_t *sample, uint32_t timeout_ms);
bool platform_uart_write_dma(const uint8_t *data, size_t length,
                             uint32_t timeout_ms);

void platform_sensor_bus_recover(void);
void platform_watchdog_start(uint32_t timeout_ms);
void platform_watchdog_feed(void);
void platform_enter_power_mode(platform_power_mode_t mode);

#endif
