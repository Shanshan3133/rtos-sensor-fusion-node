#include "app.h"

#include "app_config.h"
#include "calibration.h"
#include "fusion.h"
#include "health_monitor.h"
#include "platform.h"
#include "telemetry.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <string.h>

static QueueHandle_t imu_queue;
#if ENABLE_ENV_SENSORS
static QueueHandle_t baro_queue;
static QueueHandle_t temp_queue;
#endif
static QueueHandle_t state_queue;
static volatile uint32_t health_mask;
static volatile uint32_t sticky_status;

static StaticQueue_t imu_queue_control;
#if ENABLE_ENV_SENSORS
static StaticQueue_t baro_queue_control;
static StaticQueue_t temp_queue_control;
#endif
static StaticQueue_t state_queue_control;
static uint8_t imu_queue_storage[IMU_QUEUE_DEPTH * sizeof(imu_sample_t)];
#if ENABLE_ENV_SENSORS
static uint8_t baro_queue_storage[BARO_QUEUE_DEPTH * sizeof(baro_sample_t)];
static uint8_t temp_queue_storage[TEMP_QUEUE_DEPTH * sizeof(temp_sample_t)];
#endif
static uint8_t state_queue_storage[TELEMETRY_QUEUE_DEPTH * sizeof(fused_state_t)];

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

static void report_data_fault(uint32_t faults) {
    if ((faults & DATA_FAULT_TIMESTAMP) != 0u) {
        status_set(STATUS_TIMESTAMP_FAULT);
    }
    if ((faults & ~DATA_FAULT_TIMESTAMP) != 0u) {
        status_set(STATUS_DATA_IMPLAUSIBLE);
    }
}

void app_health_kick(uint32_t task_bit) {
    taskENTER_CRITICAL();
    health_mask |= task_bit;
    taskEXIT_CRITICAL();
}

static void replace_latest(QueueHandle_t queue, const void *item,
                           uint32_t overrun_bit) {
    if (xQueueSend(queue, item, 0u) != pdPASS) {
        uint8_t discarded[sizeof(fused_state_t)];
        (void)xQueueReceive(queue, discarded, 0u);
        (void)xQueueSend(queue, item, 0u);
        status_set(overrun_bit);
    }
}

static void imu_task(void *argument) {
    (void)argument;
    TickType_t wake = xTaskGetTickCount();
    unsigned failures = 0u;
    calibration_accumulator_t calibration_acc;
    imu_calibration_t calibration;
    bool calibration_ready = false;
    data_health_monitor_t data_health;
    data_health_init(&data_health);
    calibration_begin(&calibration_acc);
    for (;;) {
        imu_sample_t sample;
        if (platform_imu_read_dma(&sample, 2u)) {
            failures = 0u;
            const uint32_t faults = data_health_check_imu(
                &data_health, &sample, !calibration_ready);
            if (faults != DATA_FAULT_NONE) {
                report_data_fault(faults);
            } else if (!calibration_ready) {
                calibration_push(&calibration_acc, &sample);
                if (calibration_acc.count >= 2000u) {
                    calibration_ready = calibration_finish(&calibration_acc,
                                                           &calibration);
                    if (!calibration_ready) calibration_begin(&calibration_acc);
                }
            } else {
                calibration_apply(&calibration, &sample);
                status_set(STATUS_CALIBRATED);
            }
            if (faults == DATA_FAULT_NONE) {
                replace_latest(imu_queue, &sample, STATUS_IMU_OVERRUN);
            }
        } else if (++failures >= 3u) {
            platform_sensor_bus_recover();
            status_set(STATUS_SENSOR_RECOVERY);
            failures = 0u;
        }
        /* Liveness is independent of transaction/data validity. */
        app_health_kick(HEALTH_IMU);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(1000u / IMU_SAMPLE_HZ));
    }
}

#if ENABLE_ENV_SENSORS
static void environment_task(void *argument) {
    (void)argument;
    TickType_t wake = xTaskGetTickCount();
    unsigned divider = 0u;
    data_health_monitor_t data_health;
    data_health_init(&data_health);
    for (;;) {
        baro_sample_t baro;
        if (platform_baro_read_dma(&baro, 5u)) {
            const uint32_t faults = data_health_check_baro(&data_health, &baro);
            if (faults == DATA_FAULT_NONE) {
                replace_latest(baro_queue, &baro, STATUS_BARO_OVERRUN);
            } else {
                report_data_fault(faults);
            }
        }
        if ((divider++ % (BARO_SAMPLE_HZ / TEMP_SAMPLE_HZ)) == 0u) {
            temp_sample_t temp;
            if (platform_temp_read_dma(&temp, 5u)) {
                const uint32_t faults = data_health_check_temp(&data_health,
                                                               &temp);
                if (faults == DATA_FAULT_NONE) {
                    replace_latest(temp_queue, &temp, 0u);
                } else {
                    report_data_fault(faults);
                }
            }
        }
        app_health_kick(HEALTH_ENV);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(1000u / BARO_SAMPLE_HZ));
    }
}
#endif

static void fusion_task(void *argument) {
    (void)argument;
    fusion_filter_t filter;
    fused_state_t state;
#if ENABLE_ENV_SENSORS
    temp_sample_t temperature;
#endif
    TickType_t last_publish = 0u;
    memset(&state, 0, sizeof(state));
    fusion_init(&filter, 101325.0f);

    for (;;) {
        imu_sample_t imu;
        if (xQueueReceive(imu_queue, &imu, pdMS_TO_TICKS(10u)) != pdPASS) {
            app_health_kick(HEALTH_FUSION);
            continue;
        }
        (void)fusion_update_imu(&filter, &imu, &state);

#if ENABLE_ENV_SENSORS
        baro_sample_t baro;
        while (xQueueReceive(baro_queue, &baro, 0u) == pdPASS) {
            (void)fusion_update_baro(&filter, &baro, &state);
        }
        while (xQueueReceive(temp_queue, &temperature, 0u) == pdPASS) {
            state.temperature_c = temperature.temperature_c;
            state.status |= STATUS_TEMP_VALID;
        }
#else
        state.temperature_c = imu.imu_temp_c;
        state.status |= STATUS_TEMP_VALID;
#endif
        state.status |= status_get();

        const TickType_t now = xTaskGetTickCount();
        if ((now - last_publish) >= pdMS_TO_TICKS(1000u / FUSION_OUTPUT_HZ)) {
            replace_latest(state_queue, &state, STATUS_UART_DROP);
            last_publish = now;
        }
        app_health_kick(HEALTH_FUSION);
    }
}

static void telemetry_task(void *argument) {
    (void)argument;
    uint16_t sequence = 0u;
    TickType_t last_tx = 0u;
    for (;;) {
        fused_state_t state;
        if (xQueueReceive(state_queue, &state, pdMS_TO_TICKS(100u)) != pdPASS) {
            app_health_kick(HEALTH_TELEMETRY);
            continue;
        }
        const TickType_t now = xTaskGetTickCount();
        if ((now - last_tx) < pdMS_TO_TICKS(1000u / TELEMETRY_OUTPUT_HZ)) continue;

        uint8_t frame[TELEMETRY_MAX_FRAME];
        const size_t length = telemetry_encode_state(sequence, &state, frame,
                                                     sizeof(frame));
        if (length != 0u && platform_uart_write_dma(frame, length, 10u)) {
            ++sequence;
            last_tx = now;
            app_health_kick(HEALTH_TELEMETRY);
        } else {
            status_set(STATUS_UART_DROP);
        }
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
        /* Missing health intentionally causes an IWDG reset. */
    }
}

static void power_task(void *argument) {
    (void)argument;
    for (;;) {
        /* STOP policy can be extended with a command/activity event group. */
        platform_enter_power_mode(PLATFORM_POWER_IDLE);
        vTaskDelay(pdMS_TO_TICKS(100u));
    }
}

void app_start(void) {
    imu_queue = xQueueCreateStatic(IMU_QUEUE_DEPTH, sizeof(imu_sample_t),
                                   imu_queue_storage, &imu_queue_control);
    state_queue = xQueueCreateStatic(TELEMETRY_QUEUE_DEPTH,
                                     sizeof(fused_state_t),
                                     state_queue_storage,
                                     &state_queue_control);
#if ENABLE_ENV_SENSORS
    baro_queue = xQueueCreateStatic(BARO_QUEUE_DEPTH, sizeof(baro_sample_t),
                                    baro_queue_storage, &baro_queue_control);
    temp_queue = xQueueCreateStatic(TEMP_QUEUE_DEPTH, sizeof(temp_sample_t),
                                    temp_queue_storage, &temp_queue_control);
    configASSERT(imu_queue && baro_queue && temp_queue && state_queue);
#else
    configASSERT(imu_queue && state_queue);
#endif

    configASSERT(xTaskCreate(imu_task, "imu", 384u, NULL, TASK_PRIORITY_IMU,
                             NULL) == pdPASS);
    configASSERT(xTaskCreate(fusion_task, "fusion", 640u, NULL,
                             TASK_PRIORITY_FUSION, NULL) == pdPASS);
#if ENABLE_ENV_SENSORS
    configASSERT(xTaskCreate(environment_task, "env", 384u, NULL,
                             TASK_PRIORITY_ENV, NULL) == pdPASS);
#endif
    configASSERT(xTaskCreate(telemetry_task, "uart", 384u, NULL,
                             TASK_PRIORITY_TELEMETRY, NULL) == pdPASS);
    configASSERT(xTaskCreate(power_task, "power", 256u, NULL,
                             TASK_PRIORITY_POWER, NULL) == pdPASS);
    configASSERT(xTaskCreate(watchdog_task, "wdg", 256u, NULL,
                             TASK_PRIORITY_WATCHDOG, NULL) == pdPASS);
}
