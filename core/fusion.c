#include "fusion.h"

#include <math.h>
#include <stddef.h>

#define GRAVITY_MPS2 9.80665f
#define PI_F 3.14159265358979323846f

static bool finite3(const float v[3]) {
    return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]);
}

static float elapsed_seconds(uint32_t now, uint32_t then) {
    return (float)(uint32_t)(now - then) * 1.0e-6f;
}

static void angle_init(angle_kalman_t *k) {
    k->angle = 0.0f;
    k->bias = 0.0f;
    k->p00 = 1.0f;
    k->p01 = 0.0f;
    k->p10 = 0.0f;
    k->p11 = 1.0f;
    k->q_angle = 0.0025f;
    k->q_bias = 0.0001f;
    k->r_measure = 0.03f;
}

static float angle_update(angle_kalman_t *k, float measured_angle,
                          float gyro_rate, float dt) {
    const float rate = gyro_rate - k->bias;
    k->angle += dt * rate;

    k->p00 += dt * (dt * k->p11 - k->p01 - k->p10 + k->q_angle);
    k->p01 -= dt * k->p11;
    k->p10 -= dt * k->p11;
    k->p11 += k->q_bias * dt;

    const float innovation = measured_angle - k->angle;
    const float innovation_covariance = k->p00 + k->r_measure;
    if (innovation_covariance <= 1.0e-9f) {
        return k->angle;
    }

    const float gain0 = k->p00 / innovation_covariance;
    const float gain1 = k->p10 / innovation_covariance;
    const float p00 = k->p00;
    const float p01 = k->p01;
    k->angle += gain0 * innovation;
    k->bias += gain1 * innovation;
    k->p00 -= gain0 * p00;
    k->p01 -= gain0 * p01;
    k->p10 -= gain1 * p00;
    k->p11 -= gain1 * p01;
    return k->angle;
}

void fusion_init(fusion_filter_t *filter, float sea_level_pa) {
    angle_init(&filter->roll);
    angle_init(&filter->pitch);
    filter->altitude = 0.0f;
    filter->altitude_variance = 25.0f;
    filter->altitude_process_noise = 0.15f;
    filter->altitude_measurement_noise = 2.5f;
    filter->vertical_speed = 0.0f;
    filter->yaw = 0.0f;
    filter->sea_level_pa = sea_level_pa > 1000.0f ? sea_level_pa : 101325.0f;
    filter->last_imu_us = 0u;
    filter->last_baro_us = 0u;
    filter->initialized = false;
    filter->baro_initialized = false;
}

bool fusion_update_imu(fusion_filter_t *filter, const imu_sample_t *sample,
                       fused_state_t *state) {
    if (!finite3(sample->accel_mps2) || !finite3(sample->gyro_rads)) {
        state->status |= STATUS_NUMERIC_FAULT;
        return false;
    }

    float dt = filter->last_imu_us == 0u ? 1.0f / 1000.0f :
               elapsed_seconds(sample->timestamp_us, filter->last_imu_us);
    filter->last_imu_us = sample->timestamp_us;
    if (dt <= 0.0f || dt > 0.05f) {
        state->status |= STATUS_NUMERIC_FAULT;
        return false;
    }

    const float ax = sample->accel_mps2[0];
    const float ay = sample->accel_mps2[1];
    const float az = sample->accel_mps2[2];
    const float magnitude = sqrtf(ax * ax + ay * ay + az * az);
    const bool accel_trustworthy = magnitude > 0.75f * GRAVITY_MPS2 &&
                                    magnitude < 1.25f * GRAVITY_MPS2;

    const float measured_roll = atan2f(ay, az);
    const float measured_pitch = atan2f(-ax, sqrtf(ay * ay + az * az));
    if (!filter->initialized) {
        filter->roll.angle = measured_roll;
        filter->pitch.angle = measured_pitch;
        filter->initialized = true;
    }

    /* Inflate R during linear acceleration instead of accepting a false tilt. */
    filter->roll.r_measure = accel_trustworthy ? 0.03f : 30.0f;
    filter->pitch.r_measure = accel_trustworthy ? 0.03f : 30.0f;
    state->roll_rad = angle_update(&filter->roll, measured_roll,
                                   sample->gyro_rads[0], dt);
    state->pitch_rad = angle_update(&filter->pitch, measured_pitch,
                                    sample->gyro_rads[1], dt);
    /* A six-axis IMU cannot observe absolute yaw. Startup calibration removes
     * the Z gyro bias; never substitute the unrelated roll/pitch biases. */
    filter->yaw += sample->gyro_rads[2] * dt;
    if (filter->yaw > PI_F) filter->yaw -= 2.0f * PI_F;
    if (filter->yaw < -PI_F) filter->yaw += 2.0f * PI_F;
    state->yaw_rad = filter->yaw;
    state->timestamp_us = sample->timestamp_us;
    state->status |= STATUS_IMU_VALID;
    return true;
}

bool fusion_update_baro(fusion_filter_t *filter, const baro_sample_t *sample,
                        fused_state_t *state) {
    if (!isfinite(sample->pressure_pa) || sample->pressure_pa < 30000.0f ||
        sample->pressure_pa > 120000.0f) {
        state->status |= STATUS_NUMERIC_FAULT;
        return false;
    }
    const float measured = 44330.0f *
        (1.0f - powf(sample->pressure_pa / filter->sea_level_pa, 0.19029496f));
    const float dt = filter->last_baro_us == 0u ? 1.0f / 50.0f :
                     elapsed_seconds(sample->timestamp_us, filter->last_baro_us);
    filter->last_baro_us = sample->timestamp_us;
    if (dt <= 0.0f || dt > 1.0f) return false;

    if (!filter->baro_initialized) {
        filter->altitude = measured;
        filter->vertical_speed = 0.0f;
        filter->baro_initialized = true;
        state->altitude_m = measured;
        state->vertical_speed_mps = 0.0f;
        state->status |= STATUS_BARO_VALID;
        return true;
    }

    const float previous = filter->altitude;
    filter->altitude_variance += filter->altitude_process_noise * dt;
    const float gain = filter->altitude_variance /
        (filter->altitude_variance + filter->altitude_measurement_noise);
    filter->altitude += gain * (measured - filter->altitude);
    filter->altitude_variance *= (1.0f - gain);
    const float raw_speed = (filter->altitude - previous) / dt;
    filter->vertical_speed += 0.15f * (raw_speed - filter->vertical_speed);
    state->altitude_m = filter->altitude;
    state->vertical_speed_mps = filter->vertical_speed;
    state->status |= STATUS_BARO_VALID;
    return true;
}
