#include <usb.h>
#include <usb_descriptors.h>
#include "tusb.h"

#include "FreeRTOS.h"
#include "task.h"

static usbd_recv_callback_t callback;

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  // nothing to with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) return true;

  switch (request->bmRequestType_bit.type)
  {
    case TUSB_REQ_TYPE_VENDOR:
      switch (request->bRequest)
      {
        case VENDOR_REQUEST_MICROSOFT:
          if ( request->wIndex == 7 )
          {
            // Get Microsoft OS 2.0 compatible descriptor
            uint16_t total_len;
            memcpy(&total_len, desc_ms_os_20+8, 2);

            return tud_control_xfer(rhport, request, (void*)(uintptr_t) desc_ms_os_20, total_len);
          }else
          {
            return false;
          }

        default: break;
      }
    break;

    default: break;
  }

  // stall unknown request
  return false;
}

//static char *strnstr(const char *s1, const char *s2, int length) {
//	if(!s1 || !s2) {
//		return NULL;
//	}
//	int cmpLen = strlen(s2);
//	for(int i=0;i<=length-cmpLen;i++) {
//		if(strncmp(&s[i], s2, cmpLen) == 0) {
//			return &s[i];
//		}
//	}
//	return NULL;
//}

static void handleIncoming(char *buf, uint16_t *recCnt, uint8_t interface) {
	uint16_t cnt = 0;
	if(interface == USB_INTERFACE_CDC) {
		if(tud_cdc_available()) {
			cnt = tud_cdc_read(&buf[*recCnt], USB_REC_BUFFER_SIZE - *recCnt);
		}
	} else {
		if(tud_vendor_available()) {
			cnt = tud_vendor_read(&buf[*recCnt], USB_REC_BUFFER_SIZE - *recCnt);
		}
	}
	*recCnt += cnt;
	char *lineEnd;
	while(lineEnd = memchr(buf, '\n', *recCnt)) {
		uint16_t bytes = lineEnd - buf + 1;
		if(callback) {
			callback(buf, bytes, interface);
		}
		if(*recCnt > bytes) {
			// got more bytes already received
			memmove(buf, &buf[bytes], *recCnt - bytes);
			*recCnt -= bytes;
		} else {
			*recCnt = 0;
		}
	}
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
	static uint8_t buf[USB_REC_BUFFER_SIZE];
	static uint16_t recCnt = 0;

	handleIncoming(buf, &recCnt, USB_INTERFACE_CDC);
}

void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize)
{
	static uint8_t buf[USB_REC_BUFFER_SIZE];
	static uint16_t recCnt = 0;

	handleIncoming(buf, &recCnt, USB_INTERFACE_VENDOR);
}

static void tinyUSB_task(void* ptr) {
	while(true) {
		tud_task();
	}
}

void usb_init(usbd_recv_callback_t receive_callback) {
	callback = receive_callback;
	tud_init(0);
	xTaskCreate(tinyUSB_task, "TinyUSB", 1024, NULL, 1, NULL);
}
bool usb_transmit(const uint8_t *data, uint16_t length, uint8_t i) {
	if(i == USB_INTERFACE_CDC) {
		while(tud_cdc_write_available() < length) {
			vTaskDelay(1);
		}
		tud_cdc_write(data, length);
		tud_cdc_write_flush();
	} else if(i == USB_INTERFACE_VENDOR) {
		while(tud_vendor_write_available() < length) {
			vTaskDelay(1);
		}
		tud_vendor_write(data, length);
		tud_vendor_write_flush();
	}
    return true;
}
uint16_t usb_available_buffer() {

}
void usb_log(const char *log, uint16_t length) {

}
void usb_clear_buffer() {

}
