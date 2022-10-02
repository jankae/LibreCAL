#include "Switch.hpp"

#include "pico/stdlib.h"

#include <cstring>

using namespace Switch;

static const uint8_t VxPins[Switch::NumPorts][3] {
	{17, 16, 15},
	{18, 19, 20},
	{12, 13, 14},
	{11, 10, 9},
};

static Standard portStandards[Switch::NumPorts];

static bool configValid = false;

static void UpdatePins() {
	configValid = true;
	uint8_t through = 0x00;
	for(uint8_t i=0;i<Switch::NumPorts;i++) {
		switch(portStandards[i]) {
		case Standard::Open:
			gpio_put(VxPins[i][0], 0);
			gpio_put(VxPins[i][1], 0);
			gpio_put(VxPins[i][2], 0);
			break;
		case Standard::Short:
			gpio_put(VxPins[i][0], 1);
			gpio_put(VxPins[i][1], 0);
			gpio_put(VxPins[i][2], 0);
			break;
		case Standard::Load:
			gpio_put(VxPins[i][0], 1);
			gpio_put(VxPins[i][1], 0);
			gpio_put(VxPins[i][2], 1);
			break;
		case Standard::Through:
			if((through & 0x0F) == 0x00) {
				// this is the first port set to through
				through |= (i+1); // indicate the port in the lower nibble
				break;
			} else if((through & 0xF0) == 0x00) {
				// this is the second port set to through
				through |= (i+1) << 4; // indicate the second port in the upper nibble
				break;
			} else {
				// both throughs already set
				configValid = false;
				// fallthrough to 'None' setting
			}
			/* no break */
		case Standard::None:
			gpio_put(VxPins[i][0], 1);
			gpio_put(VxPins[i][1], 1);
			gpio_put(VxPins[i][2], 0);
			break;
		}
	}
	if(((through & 0x0F) == 0x00) ^ ((through & 0xF0) == 0x00)) {
		// only one port is set to through
		configValid = false;
	} else {
		// configure the through, see schematic for pin settings
		switch(through) {
		case 0x21:
			gpio_put(VxPins[0][0], 0);
			gpio_put(VxPins[0][1], 1);
			gpio_put(VxPins[0][2], 0);
			gpio_put(VxPins[1][0], 0);
			gpio_put(VxPins[1][1], 1);
			gpio_put(VxPins[1][2], 1);
			break;
		case 0x31:
			gpio_put(VxPins[0][0], 0);
			gpio_put(VxPins[0][1], 1);
			gpio_put(VxPins[0][2], 1);
			gpio_put(VxPins[2][0], 0);
			gpio_put(VxPins[2][1], 0);
			gpio_put(VxPins[2][2], 1);
			break;
		case 0x41:
			gpio_put(VxPins[0][0], 0);
			gpio_put(VxPins[0][1], 0);
			gpio_put(VxPins[0][2], 1);
			gpio_put(VxPins[3][0], 0);
			gpio_put(VxPins[3][1], 0);
			gpio_put(VxPins[3][2], 1);
			break;
		case 0x32:
			gpio_put(VxPins[1][0], 0);
			gpio_put(VxPins[1][1], 1);
			gpio_put(VxPins[1][2], 0);
			gpio_put(VxPins[2][0], 0);
			gpio_put(VxPins[2][1], 1);
			gpio_put(VxPins[2][2], 0);
			break;
		case 0x42:
			gpio_put(VxPins[1][0], 0);
			gpio_put(VxPins[1][1], 0);
			gpio_put(VxPins[1][2], 1);
			gpio_put(VxPins[3][0], 0);
			gpio_put(VxPins[3][1], 1);
			gpio_put(VxPins[3][2], 1);
			break;
		case 0x43:
			gpio_put(VxPins[2][0], 0);
			gpio_put(VxPins[2][1], 1);
			gpio_put(VxPins[2][2], 1);
			gpio_put(VxPins[3][0], 0);
			gpio_put(VxPins[3][1], 1);
			gpio_put(VxPins[3][2], 0);
			break;
		}
	}
}

void Switch::Init() {
	for(uint8_t i=0;i<Switch::NumPorts;i++) {
		portStandards[i] = Standard::None;
		for(uint8_t j=0;j<3;j++) {
			gpio_init(VxPins[i][j]);
			gpio_set_dir(VxPins[i][j], GPIO_OUT);
		}
	}
	UpdatePins();
}

void Switch::SetStandard(uint8_t port, Standard s) {
	if(port < Switch::NumPorts) {
		portStandards[port] = s;
		UpdatePins();
	}
}

Standard Switch::GetStandard(uint8_t port) {
	if(port < Switch::NumPorts) {
		return portStandards[port];
	}
}

bool Switch::isValid() {
	return configValid;
}

const char* Switch::StandardName(Standard s) {
	switch(s) {
	case Standard::Open: return "OPEN";
	case Standard::Short: return "SHORT";
	case Standard::Load: return "LOAD";
	case Standard::Through: return "THROUGH";
	case Standard::None: return "NONE";
	}
}

bool Switch::NameMatched(const char *name, Standard s) {
	auto compare = StandardName(s);
	return strcmp(name, compare) == 0;
}


