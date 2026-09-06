#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include <stdint.h>

typedef struct {
    uint32_t timestamp_us;
    float accel_mps2[3];
    float gyro_rads[3];
    float imu_temp_c;
} imu_sample_t;

typedef struct {
    uint32_t timestamp_us;
    float pressure_pa;
    float sensor_temp_c;
} baro_sample_t;

typedef struct {
    uint32_t timestamp_us;
    float temperature_c;
} temp_sample_t;

typedef struct {
    uint32_t timestamp_us;
    float roll_rad;
    float pitch_rad;
    float yaw_rad;
    float altitude_m;
    float vertical_speed_mps;
    float temperature_c;
    uint32_t status;
} fused_state_t;

enum {
    STATUS_IMU_VALID       = 1u << 0,
    STATUS_BARO_VALID      = 1u << 1,
    STATUS_TEMP_VALID      = 1u << 2,
    STATUS_CALIBRATED      = 1u << 3,
    STATUS_IMU_OVERRUN     = 1u << 4,
    STATUS_BARO_OVERRUN    = 1u << 5,
    STATUS_UART_DROP       = 1u << 6,
    STATUS_SENSOR_RECOVERY = 1u << 7,
    STATUS_WATCHDOG_RESET  = 1u << 8,
    STATUS_LOW_POWER       = 1u << 9,
    STATUS_NUMERIC_FAULT   = 1u << 10,
    STATUS_UART_RX_RESYNC  = 1u << 11,
    STATUS_DATA_IMPLAUSIBLE = 1u << 12,
    STATUS_TIMESTAMP_FAULT = 1u << 13
};

#endif
