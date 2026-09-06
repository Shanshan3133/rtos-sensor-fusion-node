#include "app.h"
#include "platform.h"

#include "FreeRTOS.h"
#include "task.h"

int main(void) {
    platform_init();
    app_start();
    vTaskStartScheduler();

    /* Reaching this point means heap allocation or scheduler startup failed. */
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task;
    (void)name;
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}

void vApplicationMallocFailedHook(void) {
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
