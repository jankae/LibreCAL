#include <usb.h>
#include <usb_descriptors.h>
#include "tusb.h"

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

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
	printf("CDC RX CB\r\n");
	(void) itf;
	if(tud_cdc_available()) {
		uint8_t buf[64];
		uint32_t count = tud_cdc_read(buf, sizeof(buf));
		if(callback) {
			callback(buf, count, USB_INTERFACE_CDC);
		}
	}
}

void tud_vendor_rx_cb(uint8_t itf)
{
	printf("CDC VENDOR CB\r\n");
	if(tud_vendor_available()) {
		uint8_t buf[64];
		uint32_t count = tud_vendor_read(buf, sizeof(buf));
		if(callback) {
			callback(buf, count, USB_INTERFACE_VENDOR);
		}
	}
}

static void tinyUSB_task(void* ptr) {
	while(true) {
		tud_task();
	}
}

void usb_init(usbd_recv_callback_t receive_callback) {
	callback = receive_callback;
	tud_init(0);
	xTaskCreate(tinyUSB_task, "TinyUSB", 1024, NULL, 5, NULL);
}
bool usb_transmit(const uint8_t *data, uint16_t length, uint8_t i) {
	printf("USB TX\r\n");
	if(i == USB_INTERFACE_CDC) {
		printf("CDC TX\r\n");
		tud_cdc_write(data, length);
		tud_cdc_write_flush();
	} else if(i == USB_INTERFACE_VENDOR) {
		printf("VENDOR TX\r\n");
		tud_vendor_write(data, length);
	}
    return true;
}
uint16_t usb_available_buffer() {

}
void usb_log(const char *log, uint16_t length) {

}
void usb_clear_buffer() {

}
