#include "telemetry.h"

#include <limits.h>
#include <math.h>

#include <string.h>

#define RAW_STATE_LEN TELEMETRY_RAW_STATE_LEN

typedef enum {
    RAW_OK,
    RAW_BAD_CRC,
    RAW_BAD_FORMAT
} raw_decode_result_t;

static void put_u16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void put_i32(uint8_t *p, int32_t value) {
    put_u32(p, (uint32_t)value);
}

static uint16_t get_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t get_i32(const uint8_t *p) {
    return (int32_t)get_u32(p);
}

static int32_t fixed(float value, float scale) {
    const double scaled = (double)value * scale;
    if (!isfinite(value)) return 0;
    if (scaled >= INT32_MAX) return INT32_MAX;
    if (scaled <= INT32_MIN) return INT32_MIN;
    return (int32_t)lround(scaled);
}

uint16_t crc16_ccitt_false(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8u; ++bit) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) :
                                    (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static bool append_escaped(uint8_t byte, uint8_t *out, size_t capacity,
                           size_t *position) {
    if (byte == TELEMETRY_SOF || byte == TELEMETRY_ESC) {
        if (*position + 2u > capacity) return false;
        out[(*position)++] = TELEMETRY_ESC;
        out[(*position)++] = byte ^ 0x20u;
    } else {
        if (*position + 1u > capacity) return false;
        out[(*position)++] = byte;
    }
    return true;
}

size_t telemetry_encode_state(uint16_t sequence, const fused_state_t *state,
                              uint8_t *output, size_t output_capacity) {
    uint8_t raw[RAW_STATE_LEN];
    raw[0] = TELEMETRY_PROTOCOL_VERSION;
    raw[1] = TELEMETRY_TYPE_STATE;
    put_u16(&raw[2], TELEMETRY_STATE_PAYLOAD_LEN);
    put_u16(&raw[4], sequence);
    put_u32(&raw[6], state->timestamp_us);
    put_u32(&raw[10], state->status);
    put_i32(&raw[14], fixed(state->roll_rad, 1.0e6f));
    put_i32(&raw[18], fixed(state->pitch_rad, 1.0e6f));
    put_i32(&raw[22], fixed(state->yaw_rad, 1.0e6f));
    put_i32(&raw[26], fixed(state->altitude_m, 1000.0f));
    put_i32(&raw[30], fixed(state->vertical_speed_mps, 1000.0f));
    put_i32(&raw[34], fixed(state->temperature_c, 1000.0f));
    /* Reserved fields keep the packet extensible without changing its length. */
    put_u32(&raw[38], 0u);
    put_u32(&raw[42], 0u);
    put_u32(&raw[46], 0u);
    put_u16(&raw[50], crc16_ccitt_false(raw, RAW_STATE_LEN - 2u));

    if (output_capacity < 2u) return 0u;
    size_t position = 0u;
    output[position++] = TELEMETRY_SOF;
    for (size_t i = 0; i < RAW_STATE_LEN; ++i) {
        if (!append_escaped(raw[i], output, output_capacity - 1u, &position)) {
            return 0u;
        }
    }
    if (position >= output_capacity) return 0u;
    output[position++] = TELEMETRY_SOF;
    return position;
}

static raw_decode_result_t decode_state_body(const uint8_t *body,
                                              size_t length,
                                              fused_state_t *state,
                                              uint16_t *sequence) {
    if (length != RAW_STATE_LEN) return RAW_BAD_FORMAT;
    if (crc16_ccitt_false(body, RAW_STATE_LEN - 2u) !=
        get_u16(&body[RAW_STATE_LEN - 2u])) return RAW_BAD_CRC;
    if (body[0] != TELEMETRY_PROTOCOL_VERSION ||
        body[1] != TELEMETRY_TYPE_STATE ||
        get_u16(&body[2]) != TELEMETRY_STATE_PAYLOAD_LEN ||
        get_u32(&body[38]) != 0u || get_u32(&body[42]) != 0u ||
        get_u32(&body[46]) != 0u) return RAW_BAD_FORMAT;

    *sequence = get_u16(&body[4]);
    state->timestamp_us = get_u32(&body[6]);
    state->status = get_u32(&body[10]);
    state->roll_rad = (float)get_i32(&body[14]) / 1.0e6f;
    state->pitch_rad = (float)get_i32(&body[18]) / 1.0e6f;
    state->yaw_rad = (float)get_i32(&body[22]) / 1.0e6f;
    state->altitude_m = (float)get_i32(&body[26]) / 1000.0f;
    state->vertical_speed_mps = (float)get_i32(&body[30]) / 1000.0f;
    state->temperature_c = (float)get_i32(&body[34]) / 1000.0f;
    return RAW_OK;
}

void telemetry_decoder_init(telemetry_decoder_t *decoder) {
    memset(decoder, 0, sizeof(*decoder));
}

telemetry_decode_result_t telemetry_decoder_feed(
    telemetry_decoder_t *decoder, uint8_t byte, fused_state_t *state,
    uint16_t *sequence) {
    if (decoder == NULL || state == NULL || sequence == NULL) {
        return TELEMETRY_DECODE_ERROR;
    }

    if (byte == TELEMETRY_SOF) {
        if (decoder->escaped) {
            ++decoder->stats.escape_errors;
            ++decoder->stats.resync_events;
            decoder->escaped = false;
            decoder->length = 0u;
            return TELEMETRY_DECODE_ERROR;
        }
        if (decoder->dropping) {
            ++decoder->stats.resync_events;
            decoder->dropping = false;
            decoder->length = 0u;
            return TELEMETRY_DECODE_ERROR;
        }
        if (decoder->length == 0u) return TELEMETRY_DECODE_NONE;

        const raw_decode_result_t result = decode_state_body(
            decoder->body, decoder->length, state, sequence);
        decoder->length = 0u;
        if (result == RAW_BAD_CRC) {
            ++decoder->stats.crc_errors;
            ++decoder->stats.resync_events;
            return TELEMETRY_DECODE_ERROR;
        }
        if (result == RAW_BAD_FORMAT) {
            ++decoder->stats.format_errors;
            ++decoder->stats.resync_events;
            return TELEMETRY_DECODE_ERROR;
        }
        if (decoder->have_sequence) {
            const uint16_t expected = (uint16_t)(decoder->last_sequence + 1u);
            decoder->stats.sequence_lost += (uint16_t)(*sequence - expected);
        }
        decoder->last_sequence = *sequence;
        decoder->have_sequence = true;
        ++decoder->stats.frames_ok;
        return TELEMETRY_DECODE_FRAME;
    }

    if (decoder->dropping) return TELEMETRY_DECODE_NONE;
    if (decoder->escaped) {
        if (byte != (TELEMETRY_SOF ^ 0x20u) &&
            byte != (TELEMETRY_ESC ^ 0x20u)) {
            ++decoder->stats.escape_errors;
            decoder->dropping = true;
            decoder->escaped = false;
            decoder->length = 0u;
            return TELEMETRY_DECODE_ERROR;
        }
        byte ^= 0x20u;
        decoder->escaped = false;
    } else if (byte == TELEMETRY_ESC) {
        decoder->escaped = true;
        return TELEMETRY_DECODE_NONE;
    }

    if (decoder->length >= sizeof(decoder->body)) {
        ++decoder->stats.overflow_errors;
        decoder->dropping = true;
        decoder->length = 0u;
        return TELEMETRY_DECODE_ERROR;
    }
    decoder->body[decoder->length++] = byte;
    return TELEMETRY_DECODE_NONE;
}
