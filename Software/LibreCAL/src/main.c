#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "USB/usb.h"


#define LED_PIN   (25)

static void defaultTask(void* ptr) {
	while(true);
}

static void usb_rx(const uint8_t *buf, uint16_t len) {
	usb_transmit(buf, len);
}

int main(void) {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    usb_init(usb_rx);

    xTaskCreate(defaultTask, "defaultTask", 1024, NULL, 3, NULL);

    vTaskStartScheduler();
    return 0;
}
