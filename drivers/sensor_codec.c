#include "sensor_codec.h"

#include <math.h>
#include <stddef.h>

#define GRAVITY_MPS2       9.80665f
#define DEG_TO_RAD         0.01745329251994329577f
#define ICM_ACCEL_LSB_G    2048.0f
#define ICM_GYRO_LSB_DPS   16.4f
#define ICM_TEMP_LSB_C     333.87f
#define ICM_TEMP_OFFSET_C  21.0f

static int16_t be_i16(const uint8_t *bytes) {
    return (int16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint16_t le_u16(const uint8_t *bytes) {
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static int16_t le_i16(const uint8_t *bytes) {
    return (int16_t)le_u16(bytes);
}

bool icm20948_decode_16g_2000dps(const uint8_t burst[14],
                                uint32_t timestamp_us,
                                imu_sample_t *sample) {
    if (burst == NULL || sample == NULL) return false;

    sample->timestamp_us = timestamp_us;
    for (size_t axis = 0u; axis < 3u; ++axis) {
        const int16_t accel_raw = be_i16(&burst[axis * 2u]);
        const int16_t gyro_raw = be_i16(&burst[6u + axis * 2u]);
        sample->accel_mps2[axis] = (float)accel_raw *
                                   (GRAVITY_MPS2 / ICM_ACCEL_LSB_G);
        sample->gyro_rads[axis] = (float)gyro_raw *
                                  (DEG_TO_RAD / ICM_GYRO_LSB_DPS);
    }
    sample->imu_temp_c = (float)be_i16(&burst[12]) / ICM_TEMP_LSB_C +
                         ICM_TEMP_OFFSET_C;
    return true;
}

float tmp117_decode_temperature(const uint8_t register_bytes[2]) {
    return (float)be_i16(register_bytes) / 128.0f;
}

bool bmp390_decode_raw(const uint8_t register_bytes[6],
                       bmp390_raw_sample_t *sample) {
    if (register_bytes == NULL || sample == NULL) return false;
    sample->pressure_adc = (uint32_t)register_bytes[0] |
                           ((uint32_t)register_bytes[1] << 8) |
                           ((uint32_t)register_bytes[2] << 16);
    sample->temperature_adc = (uint32_t)register_bytes[3] |
                              ((uint32_t)register_bytes[4] << 8) |
                              ((uint32_t)register_bytes[5] << 16);
    return true;
}

bool bmp390_parse_calibration(const uint8_t trim[21],
                              bmp390_calibration_t *calibration) {
    if (trim == NULL || calibration == NULL) return false;
    calibration->par_t1 = (double)le_u16(&trim[0]) / 0.00390625;
    calibration->par_t2 = (double)le_u16(&trim[2]) / 1073741824.0;
    calibration->par_t3 = (double)(int8_t)trim[4] / 281474976710656.0;
    calibration->par_p1 = ((double)le_i16(&trim[5]) - 16384.0) / 1048576.0;
    calibration->par_p2 = ((double)le_i16(&trim[7]) - 16384.0) / 536870912.0;
    calibration->par_p3 = (double)(int8_t)trim[9] / 4294967296.0;
    calibration->par_p4 = (double)(int8_t)trim[10] / 137438953472.0;
    calibration->par_p5 = (double)le_u16(&trim[11]) / 0.125;
    calibration->par_p6 = (double)le_u16(&trim[13]) / 64.0;
    calibration->par_p7 = (double)(int8_t)trim[15] / 256.0;
    calibration->par_p8 = (double)(int8_t)trim[16] / 32768.0;
    calibration->par_p9 = (double)le_i16(&trim[17]) / 281474976710656.0;
    calibration->par_p10 = (double)(int8_t)trim[19] / 281474976710656.0;
    calibration->par_p11 = (double)(int8_t)trim[20] /
                           36893488147419103232.0;
    return true;
}

bool bmp390_compensate(const bmp390_raw_sample_t *raw,
                       const bmp390_calibration_t *calibration,
                       float *pressure_pa,
                       float *temperature_c) {
    if (raw == NULL || calibration == NULL || pressure_pa == NULL ||
        temperature_c == NULL) return false;

    const double dt = (double)raw->temperature_adc - calibration->par_t1;
    const double temperature = dt * calibration->par_t2 +
                               dt * dt * calibration->par_t3;
    const double t2 = temperature * temperature;
    const double t3 = t2 * temperature;
    const double pressure = (double)raw->pressure_adc;
    const double p2 = pressure * pressure;
    const double p3 = p2 * pressure;
    const double offset = calibration->par_p5 +
                          calibration->par_p6 * temperature +
                          calibration->par_p7 * t2 +
                          calibration->par_p8 * t3;
    const double sensitivity = calibration->par_p1 +
                               calibration->par_p2 * temperature +
                               calibration->par_p3 * t2 +
                               calibration->par_p4 * t3;
    const double compensated = offset + pressure * sensitivity +
        p2 * (calibration->par_p9 + calibration->par_p10 * temperature) +
        p3 * calibration->par_p11;

    if (!isfinite(compensated) || !isfinite(temperature) ||
        compensated < 30000.0 || compensated > 125000.0 ||
        temperature < -50.0 || temperature > 100.0) return false;
    *pressure_pa = (float)compensated;
    *temperature_c = (float)temperature;
    return true;
}
