#include "spectrum.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define PI_F 3.14159265358979323846f

static uint16_t clamp_q15(float value) {
    if (!isfinite(value) || value <= 0.0f) return 0u;
    if (value >= 0.999969482421875f) return 32767u;
    return (uint16_t)lroundf(value * 32768.0f);
}

void spectrum_workspace_init(spectrum_workspace_t *workspace) {
    if (workspace == NULL) return;
    memset(workspace, 0, sizeof(*workspace));
    for (size_t i = 0u; i < ANALYZER_FFT_SIZE; ++i) {
        workspace->window[i] = 0.5f - 0.5f * cosf(
            (2.0f * PI_F * (float)i) / (float)(ANALYZER_FFT_SIZE - 1u));
    }
    workspace->initialized = true;
}

static void bit_reverse(float *real, float *imag) {
    size_t j = 0u;
    for (size_t i = 1u; i < ANALYZER_FFT_SIZE; ++i) {
        size_t bit = ANALYZER_FFT_SIZE >> 1u;
        while ((j & bit) != 0u) {
            j ^= bit;
            bit >>= 1u;
        }
        j ^= bit;
        if (i < j) {
            const float tr = real[i];
            const float ti = imag[i];
            real[i] = real[j];
            imag[i] = imag[j];
            real[j] = tr;
            imag[j] = ti;
        }
    }
}

static void fft_in_place(float *real, float *imag) {
    bit_reverse(real, imag);
    for (size_t length = 2u; length <= ANALYZER_FFT_SIZE; length <<= 1u) {
        const float angle = -2.0f * PI_F / (float)length;
        const float step_r = cosf(angle);
        const float step_i = sinf(angle);
        const size_t half = length >> 1u;
        for (size_t base = 0u; base < ANALYZER_FFT_SIZE; base += length) {
            float wr = 1.0f;
            float wi = 0.0f;
            for (size_t offset = 0u; offset < half; ++offset) {
                const size_t even = base + offset;
                const size_t odd = even + half;
                const float tr = wr * real[odd] - wi * imag[odd];
                const float ti = wr * imag[odd] + wi * real[odd];
                real[odd] = real[even] - tr;
                imag[odd] = imag[even] - ti;
                real[even] += tr;
                imag[even] += ti;
                const float next_wr = wr * step_r - wi * step_i;
                wi = wr * step_i + wi * step_r;
                wr = next_wr;
            }
        }
    }
}

bool spectrum_analyze_q15(spectrum_workspace_t *workspace,
                          const int16_t samples[ANALYZER_FFT_SIZE],
                          uint32_t sample_rate_hz,
                          channel_spectrum_t *result) {
    if (workspace == NULL || samples == NULL || result == NULL ||
        sample_rate_hz == 0u) return false;
    if (!workspace->initialized) spectrum_workspace_init(workspace);

    double square_sum = 0.0;
    float peak = 0.0f;
    for (size_t i = 0u; i < ANALYZER_FFT_SIZE; ++i) {
        const float normalized = (float)samples[i] / 32768.0f;
        const float absolute = fabsf(normalized);
        if (absolute > peak) peak = absolute;
        square_sum += (double)normalized * (double)normalized;
        workspace->real[i] = normalized * workspace->window[i];
        workspace->imag[i] = 0.0f;
    }

    fft_in_place(workspace->real, workspace->imag);
    memset(result, 0, sizeof(*result));
    result->rms_q15 = clamp_q15(
        sqrtf((float)(square_sum / (double)ANALYZER_FFT_SIZE)));
    result->peak_q15 = clamp_q15(peak);

    float largest = 0.0f;
    size_t largest_bin = 1u;
    float magnitudes[ANALYZER_SPECTRUM_BINS];
    for (size_t bin = 0u; bin < ANALYZER_SPECTRUM_BINS; ++bin) {
        const float raw = hypotf(workspace->real[bin], workspace->imag[bin]);
        const float amplitude = (bin == 0u) ? raw * 2.0f / ANALYZER_FFT_SIZE :
                                             raw * 4.0f / ANALYZER_FFT_SIZE;
        magnitudes[bin] = amplitude;
        result->magnitude_q15[bin] = clamp_q15(amplitude);
        if (bin > 0u && amplitude > largest) {
            largest = amplitude;
            largest_bin = bin;
        }
    }

    float interpolated_bin = (float)largest_bin;
    if (largest_bin > 0u && largest_bin + 1u < ANALYZER_SPECTRUM_BINS) {
        const float left = magnitudes[largest_bin - 1u];
        const float center = magnitudes[largest_bin];
        const float right = magnitudes[largest_bin + 1u];
        const float denominator = left - 2.0f * center + right;
        if (fabsf(denominator) > 1.0e-12f) {
            float delta = 0.5f * (left - right) / denominator;
            if (delta < -0.5f) delta = -0.5f;
            if (delta > 0.5f) delta = 0.5f;
            interpolated_bin += delta;
        }
    }
    const float frequency_hz = interpolated_bin * (float)sample_rate_hz /
                               (float)ANALYZER_FFT_SIZE;
    result->dominant_millihz = (uint32_t)lroundf(frequency_hz * 1000.0f);
    return isfinite(frequency_hz);
}
