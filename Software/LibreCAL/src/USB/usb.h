#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum {
	USB_INTERFACE_CDC = 0,
	USB_INTERFACE_VENDOR = 1,
} usb_interface_t;

typedef void(*usbd_recv_callback_t)(const uint8_t *buf, uint16_t len, usb_interface_t i);

void usb_init(usbd_recv_callback_t receive_callback);
bool usb_transmit(const uint8_t *data, uint16_t length, usb_interface_t i);
uint16_t usb_available_buffer();
void usb_log(const char *log, uint16_t length);
void usb_clear_buffer();

#ifdef __cplusplus
}
#endif
