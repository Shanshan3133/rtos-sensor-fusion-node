#include "health_monitor.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

void acquisition_health_init(acquisition_health_t *monitor) {
    if (monitor != NULL) memset(monitor, 0, sizeof(*monitor));
}

uint32_t acquisition_health_check(acquisition_health_t *monitor,
                                  const adc_dma_block_t *block,
                                  uint32_t expected_period_us) {
    if (monitor == NULL || block == NULL || block->packed_samples == NULL ||
        block->frames != ANALYZER_FFT_SIZE) return STATUS_NUMERIC_FAULT;

    uint32_t status = STATUS_ADC_RUNNING;
    if (monitor->initialized != 0u) {
        const uint32_t generation_delta = block->generation -
                                          monitor->last_generation;
        if (generation_delta != 1u) {
            status |= STATUS_BLOCK_GAP;
            monitor->dropped_blocks += (generation_delta > 1u) ?
                                       generation_delta - 1u : 1u;
        }
        const uint32_t elapsed = block->timestamp_us -
                                 monitor->last_timestamp_us;
        const uint32_t tolerance = expected_period_us / 20u + 1u;
        if (elapsed + tolerance < expected_period_us ||
            elapsed > expected_period_us + tolerance) {
            status |= STATUS_BLOCK_GAP;
            ++monitor->timing_faults;
        }
    }
    monitor->initialized = 1u;
    monitor->last_generation = block->generation;
    monitor->last_timestamp_us = block->timestamp_us;
    return status;
}

uint32_t spectrum_health_check(const spectrum_result_t *result,
                               uint32_t deadline_us) {
    if (result == NULL || result->sample_rate_hz == 0u) {
        return STATUS_NUMERIC_FAULT;
    }
    uint32_t status = 0u;
    bool signal_valid = true;
    if (result->processing_us > deadline_us) status |= STATUS_DSP_DEADLINE;
    for (unsigned channel = 0u; channel < ANALYZER_CHANNELS; ++channel) {
        if (result->channel[channel].dominant_millihz >
            (result->sample_rate_hz / 2u) * 1000u) {
            status |= STATUS_NUMERIC_FAULT;
            signal_valid = false;
        }
        if (result->channel[channel].rms_q15 < SIGNAL_MIN_RMS_Q15) {
            status |= STATUS_SIGNAL_WEAK;
            signal_valid = false;
        }
        if (result->channel[channel].clipped_samples != 0u ||
            result->channel[channel].peak_q15 >= SIGNAL_CLIP_Q15) {
            status |= STATUS_ADC_CLIPPING;
            signal_valid = false;
        }
        int16_t minimum = result->channel[channel].preview_q15[0];
        int16_t maximum = minimum;
        for (size_t i = 1u; i < ANALYZER_PREVIEW_SAMPLES; ++i) {
            const int16_t sample = result->channel[channel].preview_q15[i];
            if (sample < minimum) minimum = sample;
            if (sample > maximum) maximum = sample;
        }
        if ((int32_t)maximum - (int32_t)minimum <
            (int32_t)SIGNAL_MIN_SPAN_Q15) {
            status |= STATUS_SIGNAL_FROZEN;
            signal_valid = false;
        }
    }
    if (signal_valid) status |= STATUS_SIGNAL_VALID;
    return status;
}
