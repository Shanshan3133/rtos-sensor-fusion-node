#include "health_monitor.h"

#include <math.h>
#include <string.h>

#define GRAVITY_MPS2 9.80665f

static bool finite3(const float value[3]) {
    return isfinite(value[0]) && isfinite(value[1]) && isfinite(value[2]);
}

static bool timestamp_bad(uint32_t now, uint32_t previous,
                          uint32_t minimum_us, uint32_t maximum_us) {
    if (previous == 0u) return false;
    const uint32_t elapsed = now - previous;
    return elapsed < minimum_us || elapsed > maximum_us;
}

void data_health_init(data_health_monitor_t *monitor) {
    memset(monitor, 0, sizeof(*monitor));
}

uint32_t data_health_check_imu(data_health_monitor_t *monitor,
                               const imu_sample_t *sample,
                               bool stationary_expected) {
    uint32_t faults = DATA_FAULT_NONE;
    if (!finite3(sample->accel_mps2) || !finite3(sample->gyro_rads) ||
        !isfinite(sample->imu_temp_c)) {
        faults |= DATA_FAULT_NONFINITE;
    } else {
        const float ax = sample->accel_mps2[0];
        const float ay = sample->accel_mps2[1];
        const float az = sample->accel_mps2[2];
        const float acceleration = sqrtf(ax * ax + ay * ay + az * az);
        if (acceleration > 17.0f * GRAVITY_MPS2 ||
            sample->imu_temp_c < -40.0f || sample->imu_temp_c > 90.0f) {
            faults |= DATA_FAULT_RANGE;
        }
        if (stationary_expected &&
            (acceleration < 0.85f * GRAVITY_MPS2 ||
             acceleration > 1.15f * GRAVITY_MPS2)) {
            faults |= DATA_FAULT_STATIONARY;
        }
    }
    if (timestamp_bad(sample->timestamp_us, monitor->last_imu_us,
                      500u, 5000u)) {
        faults |= DATA_FAULT_TIMESTAMP;
        ++monitor->timestamp_faults;
    }
    monitor->last_imu_us = sample->timestamp_us;
    if (faults != DATA_FAULT_NONE) ++monitor->imu_rejected;
    return faults;
}

uint32_t data_health_check_baro(data_health_monitor_t *monitor,
                                const baro_sample_t *sample) {
    uint32_t faults = DATA_FAULT_NONE;
    if (!isfinite(sample->pressure_pa) || !isfinite(sample->sensor_temp_c)) {
        faults |= DATA_FAULT_NONFINITE;
    } else if (sample->pressure_pa < 30000.0f ||
               sample->pressure_pa > 120000.0f ||
               sample->sensor_temp_c < -40.0f ||
               sample->sensor_temp_c > 85.0f) {
        faults |= DATA_FAULT_RANGE;
    }
    if (timestamp_bad(sample->timestamp_us, monitor->last_baro_us,
                      10000u, 100000u)) {
        faults |= DATA_FAULT_TIMESTAMP;
        ++monitor->timestamp_faults;
    }
    monitor->last_baro_us = sample->timestamp_us;
    if (faults != DATA_FAULT_NONE) ++monitor->baro_rejected;
    return faults;
}

uint32_t data_health_check_temp(data_health_monitor_t *monitor,
                                const temp_sample_t *sample) {
    uint32_t faults = DATA_FAULT_NONE;
    if (!isfinite(sample->temperature_c)) {
        faults |= DATA_FAULT_NONFINITE;
    } else if (sample->temperature_c < -55.0f ||
               sample->temperature_c > 150.0f) {
        faults |= DATA_FAULT_RANGE;
    }
    if (timestamp_bad(sample->timestamp_us, monitor->last_temp_us,
                      50000u, 250000u)) {
        faults |= DATA_FAULT_TIMESTAMP;
        ++monitor->timestamp_faults;
    }
    monitor->last_temp_us = sample->timestamp_us;
    if (faults != DATA_FAULT_NONE) ++monitor->temp_rejected;
    return faults;
}
