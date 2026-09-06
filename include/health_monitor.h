#ifndef HEALTH_MONITOR_H
#define HEALTH_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

#include "sensor_types.h"

enum {
    DATA_FAULT_NONE       = 0u,
    DATA_FAULT_NONFINITE  = 1u << 0,
    DATA_FAULT_RANGE      = 1u << 1,
    DATA_FAULT_TIMESTAMP  = 1u << 2,
    DATA_FAULT_STATIONARY = 1u << 3
};

typedef struct {
    uint32_t last_imu_us;
    uint32_t last_baro_us;
    uint32_t last_temp_us;
    uint32_t imu_rejected;
    uint32_t baro_rejected;
    uint32_t temp_rejected;
    uint32_t timestamp_faults;
} data_health_monitor_t;

void data_health_init(data_health_monitor_t *monitor);
uint32_t data_health_check_imu(data_health_monitor_t *monitor,
                               const imu_sample_t *sample,
                               bool stationary_expected);
uint32_t data_health_check_baro(data_health_monitor_t *monitor,
                                const baro_sample_t *sample);
uint32_t data_health_check_temp(data_health_monitor_t *monitor,
                                const temp_sample_t *sample);

#endif
