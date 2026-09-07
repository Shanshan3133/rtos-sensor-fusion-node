#ifndef APP_H
#define APP_H

#include <stdint.h>
#include "app_config.h"

void app_start(void);
void app_health_kick(uint32_t task_bit);

enum {
    HEALTH_IMU       = 1u << 0,
    HEALTH_FUSION    = 1u << 1,
    HEALTH_ENV       = 1u << 2,
    HEALTH_TELEMETRY = 1u << 3
};

#if ENABLE_ENV_SENSORS
#define HEALTH_REQUIRED (HEALTH_IMU | HEALTH_FUSION | HEALTH_ENV | HEALTH_TELEMETRY)
#else
#define HEALTH_REQUIRED (HEALTH_IMU | HEALTH_FUSION | HEALTH_TELEMETRY)
#endif

#endif
