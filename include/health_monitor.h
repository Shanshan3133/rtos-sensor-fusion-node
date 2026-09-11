#ifndef HEALTH_MONITOR_H
#define HEALTH_MONITOR_H

#include <stdint.h>

#include "analyzer_types.h"

typedef struct {
    uint32_t last_generation;
    uint32_t last_timestamp_us;
    uint32_t dropped_blocks;
    uint32_t timing_faults;
    uint8_t initialized;
} acquisition_health_t;

void acquisition_health_init(acquisition_health_t *monitor);
uint32_t acquisition_health_check(acquisition_health_t *monitor,
                                  const adc_dma_block_t *block,
                                  uint32_t expected_period_us);
uint32_t spectrum_health_check(const spectrum_result_t *result,
                               uint32_t deadline_us);

#endif
