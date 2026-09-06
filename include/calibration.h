#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdbool.h>
#include <stddef.h>
#include "sensor_types.h"

typedef struct {
    float accel_bias_mps2[3];
    float accel_scale[3];
    float gyro_bias_rads[3];
    uint32_t crc32;
} imu_calibration_t;

typedef struct {
    double accel_sum[3];
    double accel_sq_sum[3];
    double gyro_sum[3];
    double gyro_sq_sum[3];
    size_t count;
} calibration_accumulator_t;

void calibration_begin(calibration_accumulator_t *acc);
void calibration_push(calibration_accumulator_t *acc, const imu_sample_t *sample);
bool calibration_finish(const calibration_accumulator_t *acc,
                        imu_calibration_t *result);
void calibration_apply(const imu_calibration_t *cal, imu_sample_t *sample);

#endif
