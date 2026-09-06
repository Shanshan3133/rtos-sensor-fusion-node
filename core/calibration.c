#include "calibration.h"

#include <math.h>
#include <string.h>

#define MIN_SAMPLES 1000u
#define MAX_GYRO_STDDEV_RADS 0.015f
#define MAX_ACCEL_STDDEV_MPS2 0.12f
#define MAX_GRAVITY_ERROR_MPS2 0.75f
#define GRAVITY_MPS2 9.80665f

void calibration_begin(calibration_accumulator_t *acc) {
    memset(acc, 0, sizeof(*acc));
}

void calibration_push(calibration_accumulator_t *acc, const imu_sample_t *sample) {
    for (size_t i = 0; i < 3u; ++i) {
        acc->accel_sum[i] += sample->accel_mps2[i];
        acc->accel_sq_sum[i] += (double)sample->accel_mps2[i] *
                                sample->accel_mps2[i];
        acc->gyro_sum[i] += sample->gyro_rads[i];
        acc->gyro_sq_sum[i] += (double)sample->gyro_rads[i] * sample->gyro_rads[i];
    }
    ++acc->count;
}

bool calibration_finish(const calibration_accumulator_t *acc,
                        imu_calibration_t *result) {
    if (acc->count < MIN_SAMPLES) return false;
    const double n = (double)acc->count;
    double accel_mean[3];
    for (size_t i = 0; i < 3u; ++i) {
        const double gyro_mean = acc->gyro_sum[i] / n;
        const double gyro_variance = fmax(0.0, acc->gyro_sq_sum[i] / n -
                                                gyro_mean * gyro_mean);
        accel_mean[i] = acc->accel_sum[i] / n;
        const double accel_variance = fmax(0.0, acc->accel_sq_sum[i] / n -
                                                accel_mean[i] * accel_mean[i]);
        if (sqrt(gyro_variance) > MAX_GYRO_STDDEV_RADS ||
            sqrt(accel_variance) > MAX_ACCEL_STDDEV_MPS2) return false;
        result->gyro_bias_rads[i] = (float)gyro_mean;
        result->accel_bias_mps2[i] = (float)accel_mean[i];
        result->accel_scale[i] = 1.0f;
    }
    const double gravity = sqrt(accel_mean[0] * accel_mean[0] +
                                accel_mean[1] * accel_mean[1] +
                                accel_mean[2] * accel_mean[2]);
    if (fabs(gravity - GRAVITY_MPS2) > MAX_GRAVITY_ERROR_MPS2) return false;
    /* Startup calibration assumes +Z is aligned with gravity. */
    result->accel_bias_mps2[2] -= GRAVITY_MPS2;
    result->crc32 = 0u; /* Board NVM layer fills and verifies this field. */
    return true;
}

void calibration_apply(const imu_calibration_t *cal, imu_sample_t *sample) {
    for (size_t i = 0; i < 3u; ++i) {
        sample->accel_mps2[i] = (sample->accel_mps2[i] -
                                 cal->accel_bias_mps2[i]) * cal->accel_scale[i];
        sample->gyro_rads[i] -= cal->gyro_bias_rads[i];
    }
}
