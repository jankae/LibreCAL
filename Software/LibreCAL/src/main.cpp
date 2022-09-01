#include <ff.h>
#include <serial.h>
#include <usb.h>
#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include "FreeRTOS.h"
#include "task.h"

#include "SCPI.hpp"

#define LED_PIN   (25)

FATFS fs0, fs1;
FIL fil;
FRESULT fr;


static void defaultTask(void* ptr) {
	while(true) {
		vTaskDelay(1);
	}
}

static void usb_rx(const uint8_t *buf, uint16_t len, usb_interface_t i) {
	printf("USB RX\r\n");
	SCPI::Input((const char*) buf, len, i);
}

int main(void) {
	stdio_init_all();
	gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    SCPI::Init(usb_transmit);

    printf("Unique ID: %s\r\n", getSerial());

    fr = f_mount(&fs0, "0:", 1);
    if(fr != FR_OK) {
    	printf("mount error disk0 %d\r\n", fr);
    	BYTE work[FF_MAX_SS];
    	fr = f_mkfs("0:", 0, work, sizeof(work));
    	f_setlabel("0:LibreCAL_RW");
    	printf("Formatted disk0: %d\r\n", fr);
        fr = f_mount(&fs0, "0:", 1);
        if(fr != FR_OK) {
        	printf("mount error disk0 %d\r\n", fr);
        }
    }
    fr = f_mount(&fs1, "1:", 1);
    if(fr != FR_OK) {
    	printf("mount error disk1 %d\r\n", fr);
    	BYTE work[FF_MAX_SS];
    	fr = f_mkfs("1:", 0, work, sizeof(work));
    	f_setlabel("1:LibreCAL_R");
    	printf("Formatted disk1: %d\r\n", fr);
        fr = f_mount(&fs1, "1:", 1);
        if(fr != FR_OK) {
        	printf("mount error disk1 %d\r\n", fr);
        }
    }
    /* Create a file as new */
    fr = f_open(&fil, "1:serial.txt", FA_CREATE_NEW | FA_WRITE);
    if (fr) {
    	printf("f_open: %d\r\n", fr);
    }

    /* Write a message */
    UINT bw;
    f_write(&fil, getSerial(), strlen(getSerial()), &bw);
    /* Close the file */
    f_close(&fil);

    fr = f_open(&fil, "0:readwrite.txt", FA_CREATE_NEW | FA_WRITE);
    if (fr) {
    	printf("f_open: %d\r\n", fr);
    }

    /* Write a message */
    const char *str = "You can change this";
    f_write(&fil, str, strlen(str), &bw);
    /* Close the file */
    f_close(&fil);

    usb_init(usb_rx);

    xTaskCreate(defaultTask, "defaultTask", 1024, NULL, 3, NULL);

    vTaskStartScheduler();
    return 0;
}
