#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "analyzer_types.h"

void platform_init(void);
uint32_t platform_time_us(void);
bool platform_watchdog_reset_detected(void);

/* ADC1/ADC2 dual-regular simultaneous mode, timer triggered, packed in CDR. */
bool platform_adc_start(void);
bool platform_adc_wait_block(adc_dma_block_t *block, uint32_t timeout_ms);
uint32_t platform_adc_overruns(void);
uint32_t platform_adc_generation(void);

/* USART2 TX DMA through the NUCLEO ST-LINK virtual COM port. */
bool platform_uart_write_dma(const uint8_t *data, size_t length,
                             uint32_t timeout_ms);

/* Optional coherent DAC1 test tone; PA4 is wired to PA0 and/or PA1. */
bool platform_test_signal_active(void);

void platform_watchdog_start(uint32_t timeout_ms);
void platform_watchdog_feed(void);
void platform_idle(void);

#endif
