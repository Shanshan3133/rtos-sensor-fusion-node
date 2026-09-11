#ifndef SPECTRUM_H
#define SPECTRUM_H

#include <stdbool.h>
#include <stdint.h>

#include "analyzer_types.h"

typedef struct {
#if !defined(ANALYZER_USE_CMSIS_DSP)
    float real[ANALYZER_FFT_SIZE];
    float imag[ANALYZER_FFT_SIZE];
    float window[ANALYZER_FFT_SIZE];
#endif
    bool initialized;
} spectrum_workspace_t;

void spectrum_workspace_init(spectrum_workspace_t *workspace);
bool spectrum_analyze_q15(spectrum_workspace_t *workspace,
                          const int16_t samples[ANALYZER_FFT_SIZE],
                          uint32_t sample_rate_hz,
                          channel_spectrum_t *result);

#endif
