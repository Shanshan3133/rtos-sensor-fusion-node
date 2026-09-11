#include "app.h"

#include "analyzer_types.h"
#include "app_config.h"
#include "health_monitor.h"
#include "platform.h"
#include "spectrum.h"
#include "telemetry.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <string.h>

static QueueHandle_t block_queue;
static QueueHandle_t result_queue;
static StaticQueue_t block_queue_control;
static StaticQueue_t result_queue_control;
static uint8_t block_queue_storage[2u * sizeof(adc_dma_block_t)];
static uint8_t result_queue_storage[sizeof(spectrum_result_t)];

static spectrum_workspace_t spectrum_workspace[ANALYZER_CHANNELS];
static int16_t channel_samples[ANALYZER_CHANNELS][ANALYZER_FFT_SIZE];
/* Task-owned static buffers keep large FFT results off the FreeRTOS stacks. */
static spectrum_result_t dsp_result;
static spectrum_result_t telemetry_result;
static uint8_t telemetry_frame[TELEMETRY_MAX_FRAME];
static volatile uint32_t health_mask;
static volatile uint32_t sticky_status;

static void status_set(uint32_t bits) {
    taskENTER_CRITICAL();
    sticky_status |= bits;
    taskEXIT_CRITICAL();
}

static uint32_t status_get(void) {
    taskENTER_CRITICAL();
    const uint32_t value = sticky_status;
    taskEXIT_CRITICAL();
    return value;
}

void app_health_kick(uint32_t task_bit) {
    taskENTER_CRITICAL();
    health_mask |= task_bit;
    taskEXIT_CRITICAL();
}

static void acquisition_task(void *argument) {
    (void)argument;
    acquisition_health_t monitor;
    acquisition_health_init(&monitor);
    configASSERT(platform_adc_start());
    if (platform_test_signal_active()) status_set(STATUS_TEST_SIGNAL);
    const uint32_t block_period_us =
        (ADC_DMA_FRAMES_PER_HALF * 1000000u) / ADC_SAMPLE_RATE_HZ;

    for (;;) {
        adc_dma_block_t block;
        if (!platform_adc_wait_block(&block, ADC_BLOCK_TIMEOUT_MS)) {
            status_set(STATUS_ADC_OVERRUN);
            app_health_kick(HEALTH_ACQUISITION);
            continue;
        }
        status_set(acquisition_health_check(&monitor, &block,
                                            block_period_us));
        if (platform_adc_overruns() != 0u) status_set(STATUS_ADC_OVERRUN);
        if (xQueueSend(block_queue, &block, 0u) != pdPASS) {
            status_set(STATUS_FRAME_DROPPED);
        }
        app_health_kick(HEALTH_ACQUISITION);
    }
}

static void unpack_dual_adc(const adc_dma_block_t *block) {
    for (size_t i = 0u; i < ANALYZER_FFT_SIZE; ++i) {
        const uint32_t packed = block->packed_samples[i];
        const int32_t adc1 = (int32_t)(packed & 0x0FFFu) - 2048;
        const int32_t adc2 = (int32_t)((packed >> 16) & 0x0FFFu) - 2048;
        channel_samples[0][i] = (int16_t)(adc1 << 4);
        channel_samples[1][i] = (int16_t)(adc2 << 4);
    }
}

static void dsp_task(void *argument) {
    (void)argument;
    for (unsigned channel = 0u; channel < ANALYZER_CHANNELS; ++channel) {
        spectrum_workspace_init(&spectrum_workspace[channel]);
    }
    for (;;) {
        adc_dma_block_t block;
        if (xQueueReceive(block_queue, &block,
                          pdMS_TO_TICKS(ADC_BLOCK_TIMEOUT_MS)) != pdPASS) {
            app_health_kick(HEALTH_DSP);
            continue;
        }
        memset(&dsp_result, 0, sizeof(dsp_result));
        dsp_result.timestamp_us = block.timestamp_us;
        dsp_result.generation = block.generation;
        dsp_result.sample_rate_hz = ADC_SAMPLE_RATE_HZ;
        const uint32_t started_us = platform_time_us();
        unpack_dual_adc(&block);
        for (unsigned channel = 0u; channel < ANALYZER_CHANNELS; ++channel) {
            if (!spectrum_analyze_q15(&spectrum_workspace[channel],
                                      channel_samples[channel],
                                      ADC_SAMPLE_RATE_HZ,
                                      &dsp_result.channel[channel])) {
                dsp_result.status |= STATUS_NUMERIC_FAULT;
            }
            for (size_t i = 0u; i < ANALYZER_PREVIEW_SAMPLES; ++i) {
                dsp_result.channel[channel].preview_q15[i] =
                    channel_samples[channel][i *
                        (ANALYZER_FFT_SIZE / ANALYZER_PREVIEW_SAMPLES)];
            }
        }
        dsp_result.processing_us = platform_time_us() - started_us;
        dsp_result.dropped_blocks = platform_adc_overruns();
        dsp_result.status |= status_get();
        dsp_result.status |= spectrum_health_check(&dsp_result,
                                                   DSP_DEADLINE_US);
        (void)xQueueOverwrite(result_queue, &dsp_result);
        app_health_kick(HEALTH_DSP);
    }
}

static void telemetry_task(void *argument) {
    (void)argument;
    uint16_t sequence = 0u;
    TickType_t last_output = 0u;
    for (;;) {
        if (xQueueReceive(result_queue, &telemetry_result,
                          pdMS_TO_TICKS(100u)) != pdPASS) {
            app_health_kick(HEALTH_TELEMETRY);
            continue;
        }
        const TickType_t now = xTaskGetTickCount();
        if ((now - last_output) < pdMS_TO_TICKS(1000u / SPECTRUM_OUTPUT_HZ)) {
            app_health_kick(HEALTH_TELEMETRY);
            continue;
        }
        for (uint8_t channel = 0u; channel < ANALYZER_CHANNELS; ++channel) {
            for (uint8_t chunk = 0u;
                 chunk < ANALYZER_CHUNKS_PER_CHANNEL; ++chunk) {
                const size_t length = telemetry_encode_spectrum_chunk(
                    sequence, &telemetry_result, channel, chunk,
                    telemetry_frame, sizeof(telemetry_frame));
                if (length == 0u ||
                    !platform_uart_write_dma(telemetry_frame, length,
                                             UART_FRAME_TIMEOUT_MS)) {
                    status_set(STATUS_UART_BACKPRESSURE);
                    break;
                }
                ++sequence;
            }
        }
        last_output = now;
        app_health_kick(HEALTH_TELEMETRY);
    }
}

static void watchdog_task(void *argument) {
    (void)argument;
    platform_watchdog_start(WATCHDOG_TIMEOUT_MS);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(WATCHDOG_WINDOW_MS));
        taskENTER_CRITICAL();
        const uint32_t observed = health_mask;
        health_mask = 0u;
        taskEXIT_CRITICAL();
        if ((observed & HEALTH_REQUIRED) == HEALTH_REQUIRED) {
            platform_watchdog_feed();
        }
    }
}

void app_start(void) {
    if (platform_watchdog_reset_detected()) {
        status_set(STATUS_WATCHDOG_RESET);
    }
    block_queue = xQueueCreateStatic(2u, sizeof(adc_dma_block_t),
                                     block_queue_storage,
                                     &block_queue_control);
    result_queue = xQueueCreateStatic(SPECTRUM_QUEUE_DEPTH,
                                      sizeof(spectrum_result_t),
                                      result_queue_storage,
                                      &result_queue_control);
    configASSERT(block_queue != NULL && result_queue != NULL);
    configASSERT(xTaskCreate(acquisition_task, "adc", 384u, NULL,
                             TASK_PRIORITY_ACQUISITION, NULL) == pdPASS);
    configASSERT(xTaskCreate(dsp_task, "dsp", 768u, NULL,
                             TASK_PRIORITY_DSP, NULL) == pdPASS);
    configASSERT(xTaskCreate(telemetry_task, "uart", 512u, NULL,
                             TASK_PRIORITY_TELEMETRY, NULL) == pdPASS);
    configASSERT(xTaskCreate(watchdog_task, "wdg", 256u, NULL,
                             TASK_PRIORITY_WATCHDOG, NULL) == pdPASS);
}
