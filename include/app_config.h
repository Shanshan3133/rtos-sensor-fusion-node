#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define ENABLE_ENV_SENSORS        0u

/* The no-solder reference build uses a Qwiic I2C link at 400 kHz. */
#define IMU_SAMPLE_HZ           200u
#define FUSION_OUTPUT_HZ        100u
#define BARO_SAMPLE_HZ           50u
#define TEMP_SAMPLE_HZ           10u
#define TELEMETRY_OUTPUT_HZ       50u

#define IMU_QUEUE_DEPTH            8u
#define BARO_QUEUE_DEPTH           4u
#define TEMP_QUEUE_DEPTH           2u
#define TELEMETRY_QUEUE_DEPTH      8u

#define TASK_PRIORITY_WATCHDOG     6u
#define TASK_PRIORITY_IMU          5u
#define TASK_PRIORITY_FUSION       4u
#define TASK_PRIORITY_ENV          3u
#define TASK_PRIORITY_TELEMETRY    2u
#define TASK_PRIORITY_POWER        1u

#define WATCHDOG_WINDOW_MS        250u
#define WATCHDOG_TIMEOUT_MS      1000u

#endif
