#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sensor_types.h"

#define TELEMETRY_PROTOCOL_VERSION 1u
#define TELEMETRY_TYPE_STATE       1u
#define TELEMETRY_SOF              0x7Eu
#define TELEMETRY_ESC              0x7Du
#define TELEMETRY_MAX_FRAME        96u
#define TELEMETRY_STATE_PAYLOAD_LEN 44u
#define TELEMETRY_RAW_STATE_LEN     52u

typedef struct {
    uint32_t frames_ok;
    uint32_t crc_errors;
    uint32_t format_errors;
    uint32_t overflow_errors;
    uint32_t escape_errors;
    uint32_t resync_events;
    uint32_t sequence_lost;
} telemetry_decoder_stats_t;

typedef struct {
    uint8_t body[TELEMETRY_RAW_STATE_LEN];
    size_t length;
    uint16_t last_sequence;
    bool escaped;
    bool dropping;
    bool have_sequence;
    telemetry_decoder_stats_t stats;
} telemetry_decoder_t;

typedef enum {
    TELEMETRY_DECODE_NONE,
    TELEMETRY_DECODE_FRAME,
    TELEMETRY_DECODE_ERROR
} telemetry_decode_result_t;

uint16_t crc16_ccitt_false(const uint8_t *data, size_t length);

/* Returns encoded length, or zero when output_capacity is insufficient. */
size_t telemetry_encode_state(uint16_t sequence, const fused_state_t *state,
                              uint8_t *output, size_t output_capacity);

void telemetry_decoder_init(telemetry_decoder_t *decoder);
telemetry_decode_result_t telemetry_decoder_feed(
    telemetry_decoder_t *decoder, uint8_t byte, fused_state_t *state,
    uint16_t *sequence);

#endif
