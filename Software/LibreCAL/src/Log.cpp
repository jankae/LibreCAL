#include "Log.h"

#ifdef ENABLE_UART

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "hardware/uart.h"
#include "hardware/gpio.h"

#include "FreeRTOS.h"
#include "task.h"

extern "C" {

#define MAX_LINE_LENGTH		256

static char fifo[LOG_SENDBUF_LENGTH + MAX_LINE_LENGTH];
static uint16_t fifo_write, fifo_read;

#ifdef LOG_USE_MUTEX
#include "FreeRTOS.h"
#include "semphr.h"
static StaticSemaphore_t xMutex;
static SemaphoreHandle_t mutex;
#endif

#define INC_FIFO_POS(pos, inc) do { pos = (pos + inc) % LOG_SENDBUF_LENGTH; } while(0)

static uint16_t fifo_space() {
	uint16_t used;
	if(fifo_write >= fifo_read) {
		used = fifo_write - fifo_read;
	} else {
		used = fifo_write - fifo_read + LOG_SENDBUF_LENGTH;
	}
	return LOG_SENDBUF_LENGTH - used - 1;
}

void on_uart_needs_data(void) {
	if(fifo_read != fifo_write) {
		uart_putc_raw(LOG_UART_ID, fifo[fifo_read]);
		INC_FIFO_POS(fifo_read, 1);
	} else {
		// all done, disable interrupt
		uart_set_irq_enables(LOG_UART_ID, false, false);
	}
}

void Log_Init() {
	fifo_write = 0;
	fifo_read = 0;
#ifdef LOG_USE_MUTEXES
	mutex = xSemaphoreCreateMutexStatic(&xMutex);
#endif

	// initialize the UART
	uart_init(LOG_UART_ID, 115200);
	gpio_set_function(LOG_UART_PIN, UART_FUNCSEL_NUM(LOG_UART_ID, LOG_UART_PIN));

	uart_set_hw_flow(LOG_UART_ID, false, false);
    uart_set_fifo_enabled(LOG_UART_ID, false);

	// Set up the TX interrupt
    int UART_IRQ = LOG_UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, on_uart_needs_data);
    irq_set_enabled(UART_IRQ, true);

    // we need to write something here, otherwise the handler is never called later?
    uart_puts(LOG_UART_ID, "LibreCAL log start\r\n");
}


void _log_write(const char *module, const char *level, const char *fmt, ...) {
	int written = 0;
	va_list args;
	va_start(args, fmt);
#ifdef LOG_USE_MUTEX
	if (!STM::InInterrupt()) {
		xSemaphoreTake(mutex, portMAX_DELAY);
	}
#endif
	written = snprintf(&fifo[fifo_write], MAX_LINE_LENGTH, "%05lu [%6.6s,%s]: ",
			xTaskGetTickCount(), module, level);
	written += vsnprintf(&fifo[fifo_write + written], MAX_LINE_LENGTH - written,
			fmt, args);
	written += snprintf(&fifo[fifo_write + written], MAX_LINE_LENGTH - written,
			"\r\n");

	// check if line still fits into ring buffer
	if (written > fifo_space()) {
		// unable to fit line, skip
		return;
	}

	int16_t overflow = (fifo_write + written) - LOG_SENDBUF_LENGTH;
	if (overflow > 0) {
		// printf wrote over the end of the ring buffer -> wrap around
		memmove(&fifo[0], &fifo[LOG_SENDBUF_LENGTH], overflow);
	}
	INC_FIFO_POS(fifo_write, written);
	// enable interrupt
	uart_set_irq_enables(LOG_UART_ID, false, true);
}
}

#endif

