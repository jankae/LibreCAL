#include <stdint.h>
#include <stdbool.h>
#include <pico/unique_id.h>
#include <serial.h>

static bool generated = false;
static char serial[13];

static const char base64_chars[] =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+_";

static void encode_base64(char *dest, uint8_t *data) {
	dest[0] = base64_chars[data[0] >> 2];
	dest[1] = base64_chars[(data[0] & 0x03) << 4 | (data[1] >> 4)];
	dest[2] = base64_chars[(data[1] & 0x0F) << 2 | (data[2] >> 6)];
	dest[3] = base64_chars[(data[2] & 0x3F)];
}

const char* getSerial() {
	if(!generated) {
		union {
			pico_unique_board_id_t uniqueID;
			uint8_t bytes[9];
		} ser;
		pico_get_unique_board_id(&ser.uniqueID);
		ser.bytes[8] = 0x00;
		encode_base64(serial, ser.bytes);
		encode_base64(&serial[4], &ser.bytes[3]);
		encode_base64(&serial[8], &ser.bytes[6]);
		generated = true;
	}
	return serial;
}
