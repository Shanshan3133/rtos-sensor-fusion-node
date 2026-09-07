#include "calibration.h"
#include "fusion.h"
#include "health_monitor.h"
#include "sensor_codec.h"
#include "telemetry.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_crc(void) {
    static const uint8_t vector[] = "123456789";
    assert(crc16_ccitt_false(vector, 9u) == 0x29B1u);
}

static void test_stationary_fusion(void) {
    fusion_filter_t filter;
    fused_state_t state = {0};
    fusion_init(&filter, 101325.0f);
    for (uint32_t i = 1u; i <= 5000u; ++i) {
        const imu_sample_t sample = {
            .timestamp_us = i * 1000u,
            .accel_mps2 = {0.0f, 0.0f, 9.80665f},
            .gyro_rads = {0.0f, 0.0f, 0.0f},
            .imu_temp_c = 25.0f
        };
        assert(fusion_update_imu(&filter, &sample, &state));
    }
    assert(fabsf(state.roll_rad) < 1.0e-4f);
    assert(fabsf(state.pitch_rad) < 1.0e-4f);
    assert((state.status & STATUS_IMU_VALID) != 0u);
}

static void test_baro_validation(void) {
    fusion_filter_t filter;
    fused_state_t state = {0};
    fusion_init(&filter, 101325.0f);
    const baro_sample_t valid = {20000u, 101325.0f, 25.0f};
    const baro_sample_t invalid = {40000u, 0.0f, 25.0f};
    assert(fusion_update_baro(&filter, &valid, &state));
    assert(fabsf(state.altitude_m) < 1.0e-3f);
    assert(!fusion_update_baro(&filter, &invalid, &state));
    assert((state.status & STATUS_NUMERIC_FAULT) != 0u);
}

static void test_baro_first_sample_has_no_velocity_spike(void) {
    fusion_filter_t filter;
    fused_state_t state = {0};
    fusion_init(&filter, 101325.0f);
    const baro_sample_t sample = {20000u, 90000.0f, 25.0f};
    assert(fusion_update_baro(&filter, &sample, &state));
    assert(state.altitude_m > 900.0f);
    assert(fabsf(state.vertical_speed_mps) < 1.0e-6f);
}

static void test_yaw_uses_z_rate_only(void) {
    fusion_filter_t filter;
    fused_state_t state = {0};
    fusion_init(&filter, 101325.0f);
    filter.roll.bias = 0.5f;
    filter.pitch.bias = -0.3f;
    const imu_sample_t sample = {
        .timestamp_us = 1000u,
        .accel_mps2 = {0.0f, 0.0f, 9.80665f},
        .gyro_rads = {0.0f, 0.0f, 1.0f}
    };
    assert(fusion_update_imu(&filter, &sample, &state));
    assert(fabsf(state.yaw_rad - 0.001f) < 1.0e-6f);
}

static void test_calibration_rejects_motion(void) {
    calibration_accumulator_t acc;
    imu_calibration_t cal;
    calibration_begin(&acc);
    for (uint32_t i = 0; i < 1000u; ++i) {
        imu_sample_t sample = {
            .accel_mps2 = {0.0f, 0.0f, 9.80665f},
            .gyro_rads = {(i & 1u) ? 0.1f : -0.1f, 0.0f, 0.0f}
        };
        calibration_push(&acc, &sample);
    }
    assert(!calibration_finish(&acc, &cal));
}

static void test_telemetry_capacity(void) {
    fused_state_t state = {0};
    uint8_t tiny[4];
    uint8_t frame[TELEMETRY_MAX_FRAME];
    assert(telemetry_encode_state(0u, &state, tiny, sizeof(tiny)) == 0u);
    const size_t length = telemetry_encode_state(0u, &state, frame, sizeof(frame));
    assert(length >= 54u && length <= TELEMETRY_MAX_FRAME);
    assert(frame[0] == TELEMETRY_SOF && frame[length - 1u] == TELEMETRY_SOF);
}

static telemetry_decode_result_t feed_frame(telemetry_decoder_t *decoder,
                                             const uint8_t *frame,
                                             size_t length,
                                             fused_state_t *decoded,
                                             uint16_t *sequence) {
    telemetry_decode_result_t result = TELEMETRY_DECODE_NONE;
    for (size_t i = 0u; i < length; ++i) {
        const telemetry_decode_result_t current = telemetry_decoder_feed(
            decoder, frame[i], decoded, sequence);
        if (current != TELEMETRY_DECODE_NONE) result = current;
    }
    return result;
}

static void test_firmware_decoder_and_resync(void) {
    fused_state_t source = {
        .timestamp_us = 123456u,
        .roll_rad = 0.25f,
        .pitch_rad = -0.5f,
        .temperature_c = 25.125f,
        .status = STATUS_IMU_VALID
    };
    uint8_t frame[TELEMETRY_MAX_FRAME];
    size_t length = telemetry_encode_state(10u, &source, frame, sizeof(frame));
    assert(length != 0u);

    telemetry_decoder_t decoder;
    telemetry_decoder_init(&decoder);
    fused_state_t decoded = {0};
    uint16_t sequence = 0u;
    assert(feed_frame(&decoder, frame, length, &decoded, &sequence) ==
           TELEMETRY_DECODE_FRAME);
    assert(sequence == 10u);
    assert(fabsf(decoded.roll_rad - source.roll_rad) < 1.0e-6f);

    /* Oversized garbage is dropped until a delimiter, then a valid frame
     * must decode without resetting the state machine. */
    assert(telemetry_decoder_feed(&decoder, TELEMETRY_SOF, &decoded,
                                  &sequence) == TELEMETRY_DECODE_NONE);
    for (size_t i = 0u; i <= TELEMETRY_RAW_STATE_LEN; ++i) {
        (void)telemetry_decoder_feed(&decoder, 0x55u, &decoded, &sequence);
    }
    (void)telemetry_decoder_feed(&decoder, TELEMETRY_SOF, &decoded, &sequence);
    length = telemetry_encode_state(13u, &source, frame, sizeof(frame));
    assert(feed_frame(&decoder, frame, length, &decoded, &sequence) ==
           TELEMETRY_DECODE_FRAME);
    assert(decoder.stats.overflow_errors == 1u);
    assert(decoder.stats.resync_events == 1u);
    assert(decoder.stats.sequence_lost == 2u);

    const uint8_t bad_escape[] = {TELEMETRY_SOF, TELEMETRY_ESC, 0x00u,
                                  TELEMETRY_SOF};
    assert(feed_frame(&decoder, bad_escape, sizeof(bad_escape), &decoded,
                      &sequence) == TELEMETRY_DECODE_ERROR);
    assert(decoder.stats.escape_errors == 1u);
}

static void test_sensor_codecs(void) {
    const uint8_t icm[14] = {
        0x08u, 0x00u, 0xF8u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u,
        0x00u, 0x00u
    };
    imu_sample_t sample;
    assert(icm20948_decode_16g_2000dps(icm, 1234u, &sample));
    assert(fabsf(sample.accel_mps2[0] - 9.80665f) < 1.0e-4f);
    assert(fabsf(sample.accel_mps2[1] + 9.80665f) < 1.0e-4f);
    assert(sample.gyro_rads[2] > 0.0f);
    assert(fabsf(sample.imu_temp_c - 21.0f) < 1.0e-6f);
    assert(sample.timestamp_us == 1234u);

    const uint8_t tmp_positive[2] = {0x0Cu, 0x80u};
    const uint8_t tmp_negative[2] = {0xF6u, 0x00u};
    assert(fabsf(tmp117_decode_temperature(tmp_positive) - 25.0f) < 1.0e-6f);
    assert(fabsf(tmp117_decode_temperature(tmp_negative) + 20.0f) < 1.0e-6f);

    const uint8_t bmp[6] = {0x56u, 0x34u, 0x12u, 0xEFu, 0xCDu, 0xABu};
    bmp390_raw_sample_t raw;
    assert(bmp390_decode_raw(bmp, &raw));
    assert(raw.pressure_adc == 0x123456u);
    assert(raw.temperature_adc == 0xABCDEFu);

    bmp390_calibration_t calibration = {0};
    calibration.par_p5 = 100000.0;
    const bmp390_raw_sample_t compensated_raw = {500000u, 500000u};
    float pressure;
    float temperature;
    assert(bmp390_compensate(&compensated_raw, &calibration,
                             &pressure, &temperature));
    assert(fabsf(pressure - 100000.0f) < 0.1f);
    assert(fabsf(temperature) < 1.0e-6f);
}

static void test_data_health_is_separate_from_liveness(void) {
    data_health_monitor_t monitor;
    data_health_init(&monitor);
    imu_sample_t sample = {
        .timestamp_us = 1000u,
        .accel_mps2 = {0.0f, 0.0f, 9.80665f},
        .gyro_rads = {0.0f, 0.0f, 0.0f},
        .imu_temp_c = 25.0f
    };
    assert(data_health_check_imu(&monitor, &sample, true) == DATA_FAULT_NONE);
    sample.timestamp_us = 2000u;
    sample.accel_mps2[2] = 12.0f;
    assert((data_health_check_imu(&monitor, &sample, true) &
            DATA_FAULT_STATIONARY) != 0u);
    sample.timestamp_us = 2000u;
    sample.accel_mps2[2] = 9.80665f;
    assert((data_health_check_imu(&monitor, &sample, false) &
            DATA_FAULT_TIMESTAMP) != 0u);
    assert(monitor.imu_rejected == 2u);
    assert(monitor.timestamp_faults == 1u);
}

int main(void) {
    test_crc();
    test_stationary_fusion();
    test_baro_validation();
    test_baro_first_sample_has_no_velocity_spike();
    test_yaw_uses_z_rate_only();
    test_calibration_rejects_motion();
    test_telemetry_capacity();
    test_firmware_decoder_and_resync();
    test_sensor_codecs();
    test_data_health_is_separate_from_liveness();
    puts("core tests: PASS");
    return 0;
}
