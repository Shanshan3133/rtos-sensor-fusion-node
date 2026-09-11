/*
 * Target FFT backend. Compile this file instead of core/spectrum.c and define
 * ARM_MATH_CM4 when CMSIS-DSP is present in the STM32CubeIDE project.
 */
#include "spectrum.h"

#include "arm_math.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static arm_rfft_instance_q15 fft_instance;
static q15_t window_q15[ANALYZER_FFT_SIZE];
static q15_t windowed_q15[ANALYZER_FFT_SIZE];
static q15_t fft_output_q15[2u * ANALYZER_FFT_SIZE];
static q15_t magnitude_q15[ANALYZER_FFT_SIZE];

void spectrum_workspace_init(spectrum_workspace_t *workspace) {
    if (workspace == NULL) return;
    memset(workspace, 0, sizeof(*workspace));
    for (size_t i = 0u; i < ANALYZER_FFT_SIZE; ++i) {
        const float value = 0.5f - 0.5f * arm_cos_f32(
            (2.0f * PI * (float)i) / (float)(ANALYZER_FFT_SIZE - 1u));
        window_q15[i] = (q15_t)lroundf(value * 32767.0f);
    }
    workspace->initialized =
        arm_rfft_init_q15(&fft_instance, ANALYZER_FFT_SIZE, 0u, 1u) ==
        ARM_MATH_SUCCESS;
}

bool spectrum_analyze_q15(spectrum_workspace_t *workspace,
                          const int16_t samples[ANALYZER_FFT_SIZE],
                          uint32_t sample_rate_hz,
                          channel_spectrum_t *result) {
    if (workspace == NULL || samples == NULL || result == NULL ||
        sample_rate_hz == 0u) return false;
    if (!workspace->initialized) spectrum_workspace_init(workspace);
    if (!workspace->initialized) return false;

    memset(result, 0, sizeof(*result));
    int64_t square_sum = 0;
    uint16_t peak = 0u;
    for (size_t i = 0u; i < ANALYZER_FFT_SIZE; ++i) {
        const int32_t value = samples[i];
        const uint16_t absolute = (uint16_t)(value < 0 ? -value : value);
        if (absolute > peak) peak = absolute;
        square_sum += (int64_t)value * value;
    }
    result->peak_q15 = peak > 32767u ? 32767u : peak;
    result->rms_q15 = (uint16_t)lroundf(sqrtf(
        (float)square_sum / (float)ANALYZER_FFT_SIZE));

    arm_mult_q15((const q15_t *)samples, window_q15, windowed_q15,
                 ANALYZER_FFT_SIZE);
    arm_rfft_q15(&fft_instance, windowed_q15, fft_output_q15);
    arm_cmplx_mag_q15(fft_output_q15, magnitude_q15,
                      ANALYZER_SPECTRUM_BINS);

    uint16_t largest = 0u;
    size_t largest_bin = 1u;
    for (size_t bin = 0u; bin < ANALYZER_SPECTRUM_BINS; ++bin) {
        /* Undo 2.14 magnitude format and the Hann window's coherent gain. */
        const uint32_t corrected = (uint16_t)magnitude_q15[bin] * 8u;
        result->magnitude_q15[bin] = corrected > 32767u ?
                                            32767u : (uint16_t)corrected;
        if (bin > 0u && result->magnitude_q15[bin] > largest) {
            largest = result->magnitude_q15[bin];
            largest_bin = bin;
        }
    }

    float interpolated_bin = (float)largest_bin;
    if (largest_bin > 0u && largest_bin + 1u < ANALYZER_SPECTRUM_BINS) {
        const float left = result->magnitude_q15[largest_bin - 1u];
        const float center = result->magnitude_q15[largest_bin];
        const float right = result->magnitude_q15[largest_bin + 1u];
        const float denominator = left - 2.0f * center + right;
        if (fabsf(denominator) > 0.5f) {
            float delta = 0.5f * (left - right) / denominator;
            if (delta < -0.5f) delta = -0.5f;
            if (delta > 0.5f) delta = 0.5f;
            interpolated_bin += delta;
        }
    }
    result->dominant_millihz = (uint32_t)lroundf(
        interpolated_bin * (float)sample_rate_hz * 1000.0f /
        (float)ANALYZER_FFT_SIZE);
    return true;
}
