#ifndef APP_H
#define APP_H

#include <stdint.h>
void app_start(void);
void app_health_kick(uint32_t task_bit);

enum {
    HEALTH_ACQUISITION = 1u << 0,
    HEALTH_DSP         = 1u << 1,
    HEALTH_TELEMETRY   = 1u << 2
};

#define HEALTH_REQUIRED \
    (HEALTH_ACQUISITION | HEALTH_DSP | HEALTH_TELEMETRY)

#endif
