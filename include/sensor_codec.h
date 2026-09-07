#ifndef SENSOR_CODEC_H
#define SENSOR_CODEC_H

#include <stdbool.h>
#include <stdint.h>

#include "sensor_types.h"

#define ICM20948_WHO_AM_I_VALUE 0xEAu
#define BMP390_CHIP_ID_VALUE    0x60u

typedef struct {
    uint32_t pressure_adc;
    uint32_t temperature_adc;
} bmp390_raw_sample_t;

typedef struct {
    double par_t1, par_t2, par_t3;
    double par_p1, par_p2, par_p3, par_p4, par_p5, par_p6;
    double par_p7, par_p8, par_p9, par_p10, par_p11;
} bmp390_calibration_t;

/* Decode a 14-byte burst beginning at ICM-20948 ACCEL_XOUT_H (bank 0, 0x2D).
 * The selected ranges must be +/-16 g and +/-2000 degrees/s. */
bool icm20948_decode_16g_2000dps(const uint8_t burst[14],
                                uint32_t timestamp_us,
                                imu_sample_t *sample);

/* TMP117 temperature register is a signed Q8.7 value, MSB first. */
float tmp117_decode_temperature(const uint8_t register_bytes[2]);

/* BMP390 data registers are unsigned 24-bit little-endian ADC values.
 * Compensation is applied in the platform driver after reading trim data. */
bool bmp390_decode_raw(const uint8_t register_bytes[6],
                       bmp390_raw_sample_t *sample);

/* Parse the 21-byte trim block beginning at register 0x31. */
bool bmp390_parse_calibration(const uint8_t trim[21],
                              bmp390_calibration_t *calibration);

/* Apply the Bosch floating-point compensation sequence. */
bool bmp390_compensate(const bmp390_raw_sample_t *raw,
                       const bmp390_calibration_t *calibration,
                       float *pressure_pa,
                       float *temperature_c);

#endif
