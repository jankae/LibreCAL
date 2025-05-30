#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	MODE_DEFAULT,
	MODE_SIGLENT,
} ecal_mode_t;

ecal_mode_t getMode();

#ifdef __cplusplus
}
#endif
