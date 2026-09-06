#ifndef FUSION_H
#define FUSION_H

#include <stdbool.h>
#include "sensor_types.h"

typedef struct {
    float angle;
    float bias;
    float p00;
    float p01;
    float p10;
    float p11;
    float q_angle;
    float q_bias;
    float r_measure;
} angle_kalman_t;

typedef struct {
    angle_kalman_t roll;
    angle_kalman_t pitch;
    float altitude;
    float altitude_variance;
    float altitude_process_noise;
    float altitude_measurement_noise;
    float vertical_speed;
    float yaw;
    float sea_level_pa;
    uint32_t last_imu_us;
    uint32_t last_baro_us;
    bool initialized;
    bool baro_initialized;
} fusion_filter_t;

void fusion_init(fusion_filter_t *filter, float sea_level_pa);
bool fusion_update_imu(fusion_filter_t *filter, const imu_sample_t *sample,
                       fused_state_t *state);
bool fusion_update_baro(fusion_filter_t *filter, const baro_sample_t *sample,
                        fused_state_t *state);

#endif
