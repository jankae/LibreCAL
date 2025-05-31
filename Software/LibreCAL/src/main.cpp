#include <ff.h>
#include <main.h>
#include <main.h>
#include <serial.h>
#include <usb.h>
#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/spi.h"
#include "hardware/rtc.h"

#include "FreeRTOS.h"
#include "task.h"

#include "Switch.hpp"
#include "SCPI.hpp"
#include "Flash.hpp"
#include "UserInterface.hpp"
#include "Heater.hpp"

#define LOG_LEVEL	LOG_LEVEL_INFO
#define LOG_MODULE	"App"
#include "Log.h"

const char* date = __DATE__;
const char* time = __TIME__;

#define YEAR(date)  (date[7] == '?' ? 0 : ((date[7] - '0') * 1000) + \
					 (date[8] - '0') * 100 + (date[9] - '0') * 10 + \
					 (date[10] - '0'))

#define MONTH(date) (date[2] == 'n' ? (date [1] == 'a' ? 1 : 6) \
					: date[2] == 'b' ? 2 \
					: date[2] == 'r' ? (date [0] == 'M' ? 3 : 4) \
					: date[2] == 'y' ? 5 \
					: date[2] == 'l' ? 7 \
					: date[2] == 'g' ? 8 \
					: date[2] == 'p' ? 9 \
					: date[2] == 't' ? 10 \
					: date[2] == 'v' ? 11 : 12)

#define DAY(date)   ((date[4] == ' ' ? 0 : (date[4] - '0') * 10) + (date[5] - '0'))
#define HOUR(time)  ((time[0] - '0') * 10 + (time[1] - '0'))
#define MINUTE(time) ((time[3] - '0') * 10 + (time[4] - '0'))
#define SECOND(time) ((time[6] - '0') * 10 + (time[7] - '0'))


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

static ecal_usb_mode_t mode = MODE_DEFAULT;

ecal_usb_mode_t getUsbMode() {
	return mode;
}

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

bool createInfoFile() {
	/* Create a file as new */
	fr = f_open(&fil, "1:info.txt", FA_CREATE_ALWAYS | FA_WRITE);
	if (fr) {
//    	printf("f_open: %d\r\n", fr);
		return false;
	}

	/* Write a message */
	UINT bw;
	f_printf(&fil, "Serial: ");
	f_write(&fil, getSerial(), strlen(getSerial()), &bw);
	f_printf(&fil, "\r\nFirmware: %d.%d.%d\r\n", FW_MAJOR, FW_MINOR, FW_PATCH);
	f_printf(&fil, "Number of populated ports: %d", 4);
	/* Close the file */
	f_close(&fil);
	return true;
}

static void defaultTask(void* ptr) {
	handle = xTaskGetCurrentTaskHandle();

	fr = f_mount(&fs0, "0:", 1);
	if(fr != FR_OK) {
		BYTE work[FF_MAX_SS];
		f_mkfs("0:", 0, work, sizeof(work));
		f_mount(&fs0, "0:", 1);
		f_setlabel("0:LibreCAL_RW");
	}
	fr = f_mount(&fs1, "1:", 1);
	if(fr != FR_OK) {
		BYTE work[FF_MAX_SS];
		f_mkfs("1:", 0, work, sizeof(work));
		f_mount(&fs1, "1:", 1);
		f_setlabel("1:LibreCAL_R");
	}
	// Check info file
	fr = f_open(&fil, "1:info.txt", FA_OPEN_EXISTING | FA_READ);
	if (fr) {
		// failed to open, probably does not exist
		createInfoFile();
	} else {
		bool correctFirmware = false;
		char line[128];
		while(f_gets(line, sizeof(line), &fil)) {
			int major, minor, patch;
			if(sscanf(line, "Firmware: %d.%d.%d", &major, &minor, &patch) == 3) {
				// check firmware
				if(major == FW_MAJOR && minor == FW_MINOR && patch == FW_PATCH) {
					correctFirmware = true;
				}
				break;
			}
		}
		if(!correctFirmware) {
			f_close(&fil);
			createInfoFile();
		}
	}
	
	// Figure out which mode we should be in. The default mode is the normal LibreCAL mode.
	// You can always force that mode by pressing the function button when power is applied.
	// If that button is not pressed, the LibreCAL may emulate other electronic calibration
	// units it the necessary files are available
	if(UserInterface::IsFunctionHeld()) {
		mode = MODE_DEFAULT;
	} else {
		// default mode is not forced, check files
		if(!f_open(&fil, "0:siglent/info.dat", FA_OPEN_EXISTING | FA_READ)) {
			// we have data in Siglent format
			mode = MODE_SIGLENT;
			f_close(&fil);
		}
	}

	usb_init(usb_rx);

	while(true) {
		uint32_t notification;
		if(xTaskNotifyWait(0, 0, &notification, portMAX_DELAY)) {
			SCPI::Input(usb_buffer, usb_len, usb_interface);
		}
	}
}

static TaskHandle_t main_task;

int main(void) {
#ifdef ENABLE_UART
	Log_Init();
	LOG_INFO("Start");
#endif
	spi_init(spi0, 20000 * 1000);
	gpio_set_function(FLASH_CLK_PIN, GPIO_FUNC_SPI);
	gpio_set_function(FLASH_MOSI_PIN, GPIO_FUNC_SPI);
	gpio_set_function(FLASH_MISO_PIN, GPIO_FUNC_SPI);
	gpio_init(FLASH_CS_PIN);
	gpio_set_dir(FLASH_CS_PIN, GPIO_OUT);
	gpio_put(FLASH_CS_PIN, true);

	// For default Date / Time use the preprocessor macro __DATE__ / __TIME__
	int16_t year = YEAR(date);
	int8_t month = MONTH(date);
	int8_t day = DAY(date);
	int8_t hour = HOUR(time);
	int8_t minute = MINUTE(time);
	int8_t second = SECOND(time);

	datetime_t t = {
			.year  = year,
			.month = month,
			.day   = day,
			.dotw  = 0, // 0 is Sunday, so 5 is Friday => not used
			.hour  = hour,
			.min   = minute,
			.sec   = second
	};
	// Start the RTC
	rtc_init();
	rtc_set_datetime(&t);
	// clk_sys is >2000x faster than clk_rtc, so datetime is not updated immediately when rtc_get_datetime() is called.
	// the delay is up to 3 RTC clock cycles (which is 64us with the default clock settings)
	busy_wait_us(64);

	Switch::Init();
	UserInterface::Init();
	Heater::Init();
	Heater::SetTarget(35);
	SCPI::Init(usb_transmit);

	xTaskCreate(defaultTask, "defaultTask", 16384, NULL, 3, &main_task);

	vTaskStartScheduler();
	return 0;
}

