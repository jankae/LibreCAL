#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	MODE_DEFAULT,
	MODE_SIGLENT,
} ecal_usb_mode_t;

ecal_usb_mode_t getUsbMode();

#ifdef __cplusplus
}
#endif
