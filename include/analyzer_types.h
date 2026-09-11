#ifndef ANALYZER_TYPES_H
#define ANALYZER_TYPES_H

#include <stdint.h>

#define ANALYZER_CHANNELS 2u
#define ANALYZER_FFT_SIZE 1024u
#define ANALYZER_SPECTRUM_BINS (ANALYZER_FFT_SIZE / 2u)
#define ANALYZER_BINS_PER_CHUNK 128u
#define ANALYZER_PREVIEW_SAMPLES 128u
#define ANALYZER_PREVIEW_PER_CHUNK 32u
#define ANALYZER_CHUNKS_PER_CHANNEL \
    (ANALYZER_SPECTRUM_BINS / ANALYZER_BINS_PER_CHUNK)

typedef struct {
    const uint32_t *packed_samples;
    uint32_t timestamp_us;
    uint32_t generation;
    uint16_t frames;
} adc_dma_block_t;

typedef struct {
    uint16_t magnitude_q15[ANALYZER_SPECTRUM_BINS];
    int16_t preview_q15[ANALYZER_PREVIEW_SAMPLES];
    uint16_t rms_q15;
    uint16_t peak_q15;
    uint32_t dominant_millihz;
} channel_spectrum_t;

typedef struct {
    uint32_t timestamp_us;
    uint32_t generation;
    uint32_t sample_rate_hz;
    uint32_t processing_us;
    uint32_t dropped_blocks;
    uint32_t status;
    channel_spectrum_t channel[ANALYZER_CHANNELS];
} spectrum_result_t;

enum {
    STATUS_ADC_RUNNING       = 1u << 0,
    STATUS_SIGNAL_VALID      = 1u << 1,
    STATUS_ADC_OVERRUN       = 1u << 2,
    STATUS_BLOCK_GAP         = 1u << 3,
    STATUS_DSP_DEADLINE      = 1u << 4,
    STATUS_UART_BACKPRESSURE = 1u << 5,
    STATUS_NUMERIC_FAULT     = 1u << 6,
    STATUS_WATCHDOG_RESET    = 1u << 7,
    STATUS_FRAME_DROPPED     = 1u << 8,
    STATUS_TEST_SIGNAL       = 1u << 9
};

#endif
