#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Reference values for STM32F446 at 180 MHz; merge with the board project. */
#include <stdint.h>
extern uint32_t SystemCoreClock;

#define configUSE_PREEMPTION                    1
#define configUSE_TICKLESS_IDLE                 1
#define configCPU_CLOCK_HZ                      (SystemCoreClock)
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    7
#define configMINIMAL_STACK_SIZE                128
#define configTOTAL_HEAP_SIZE                   (24u * 1024u)
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configUSE_MUTEXES                       1
#define configQUEUE_REGISTRY_SIZE               8
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     0
#define configUSE_TRACE_FACILITY                1
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

#define configPRIO_BITS                         4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY         (15u << (8u - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (5u << (8u - configPRIO_BITS))

#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_uxTaskGetStackHighWaterMark     1

#define configASSERT(condition) do { if (!(condition)) { \
    __disable_irq(); for (;;) { } } } while (0)

#endif
