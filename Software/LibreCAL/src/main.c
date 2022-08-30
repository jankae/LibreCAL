#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"


#define LED_PIN   (25)

static void defaultTask(void* ptr) {
	while(true);
}

int main(void) {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    xTaskCreate(defaultTask, "defaultTask", 1024, NULL, 3, NULL);

    vTaskStartScheduler();
    return 0;
}
