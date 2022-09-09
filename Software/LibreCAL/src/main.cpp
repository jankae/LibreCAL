#include <ff.h>
#include <serial.h>
#include <usb.h>
#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/spi.h"

#include "FreeRTOS.h"
#include "task.h"

#include "SCPI.hpp"
#include "Flash.hpp"

#define LED_PIN   		25
#define FLASH_CLK_PIN	2
#define FLASH_MISO_PIN	0
#define FLASH_MOSI_PIN	3
#define FLASH_CS_PIN	1

FATFS fs0, fs1;
FIL fil;
FRESULT fr;

static char usb_buffer[256];
static uint16_t usb_len;
static usb_interface_t usb_interface;
static xTaskHandle handle;

Flash flash(spi0, FLASH_CLK_PIN, FLASH_MOSI_PIN, FLASH_MISO_PIN, FLASH_CS_PIN);

static void usb_rx(const uint8_t *buf, uint16_t len, usb_interface_t i) {
	if(len > sizeof(usb_buffer)) {
		// line too long
		return;
	}
	memcpy(usb_buffer, buf, len);
	usb_len = len;
	usb_interface = i;
	xTaskNotify(handle, 0x00, eNoAction);
}

static void defaultTask(void* ptr) {
	handle = xTaskGetCurrentTaskHandle();

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
    fr = f_open(&fil, "1:info.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (fr) {
    	printf("f_open: %d\r\n", fr);
    }

    /* Write a message */
    UINT bw;
    f_printf(&fil, "Serial: ");
    f_write(&fil, getSerial(), strlen(getSerial()), &bw);
    f_printf(&fil, "\r\nFirmware: %d.%d.%d\r\n", FW_MAJOR, FW_MINOR, FW_PATCH);
    f_printf(&fil, "Number of populated ports: %d", 4);
    /* Close the file */
    f_close(&fil);

    usb_init(usb_rx);

	while(true) {
		uint32_t notification;
		if(xTaskNotifyWait(0, 0, &notification, portMAX_DELAY)) {
			SCPI::Input(usb_buffer, usb_len, usb_interface);
		}
	}
}

int main(void) {
//	stdio_init_all();
	gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, true);

    spi_init(spi0, 20000 * 1000);
    gpio_set_function(FLASH_CLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(FLASH_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(FLASH_MISO_PIN, GPIO_FUNC_SPI);
    gpio_init(FLASH_CS_PIN);
    gpio_set_dir(FLASH_CS_PIN, GPIO_OUT);
    gpio_put(FLASH_CS_PIN, true);

    xTaskCreate(defaultTask, "defaultTask", 16384, NULL, 3, NULL);

    vTaskStartScheduler();
    return 0;
}
