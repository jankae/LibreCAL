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
static uint8_t throughDestinations[Switch::NumPorts];

static bool throughMap[4][4][3] = {
		{
				{true,true,false},	// P1->1, invalid, turn off
				{false,true,false}, // P1->2
				{false,true,true},	// P1->3
				{false,false,true},	// P1->4
		},
		{
				{false,true,true},	// P2->1
				{true,true,false},  // P2->2, invalid, turn off
				{false,true,false},	// P2->3
				{false,false,true},	// P2->4
		},
		{
				{false,false,true},	// P3->1
				{false,true,false}, // P3->2
				{true,true,false},	// P3->3, invalid, turn off
				{false,true,true},	// P3->4
		},
		{
				{false,false,true},	// P4->1
				{false,true,true},  // P4->2
				{false,true,false},	// P4->3
				{true,true,false},	// P4->4, invalid, turn off
		},
};

static void UpdatePins() {
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
			gpio_put(VxPins[i][0], throughMap[i][throughDestinations[i]][0]);
			gpio_put(VxPins[i][1], throughMap[i][throughDestinations[i]][1]);
			gpio_put(VxPins[i][2], throughMap[i][throughDestinations[i]][2]);
			break;
		case Standard::None:
			gpio_put(VxPins[i][0], 1);
			gpio_put(VxPins[i][1], 1);
			gpio_put(VxPins[i][2], 0);
			break;
		}
	}
}

void Switch::Init() {
	for(uint8_t i=0;i<Switch::NumPorts;i++) {
		portStandards[i] = Standard::None;
		throughDestinations[i] = 0;
		for(uint8_t j=0;j<3;j++) {
			gpio_init(VxPins[i][j]);
			gpio_set_dir(VxPins[i][j], GPIO_OUT);
		}
	}
	UpdatePins();
}

void Switch::SetStandard(uint8_t port, Standard s) {
	if(s == Standard::Through) {
		// must be set by SetThrough
		return;
	}
	if(port < Switch::NumPorts) {
		if(portStandards[port] == Standard::Through) {
			// also reset the destination port
			portStandards[throughDestinations[port]] = Standard::None;
			throughDestinations[throughDestinations[port]] = 0;
			throughDestinations[port] = 0;
		}
		portStandards[port] = s;
		UpdatePins();
	}
}

Standard Switch::GetStandard(uint8_t port) {
	if(port < Switch::NumPorts) {
		return portStandards[port];
	}
	return Standard::None;
}

const char* Switch::StandardName(Standard s) {
	switch(s) {
	case Standard::Open: return "OPEN";
	case Standard::Short: return "SHORT";
	case Standard::Load: return "LOAD";
	case Standard::Through: return "THROUGH";
	case Standard::None:
	default:
		return "NONE";
	}
}

bool Switch::NameMatched(const char *name, Standard s) {
	auto compare = StandardName(s);
	return strcmp(name, compare) == 0;
}

bool Switch::SetThrough(uint8_t port, uint8_t dest) {
	if(port == dest) {
		// can't set a through to itself
		return false;
	}
	if(port >= Switch::NumPorts || dest >= Switch::NumPorts) {
		// invalid number
		return false;
	}
	// Reset first, this also takes care of the destination port if this port was set to through
	SetStandard(port, Standard::None);
	SetStandard(dest, Standard::None);
	portStandards[port] = Standard::Through;
	portStandards[dest] = Standard::Through;
	throughDestinations[port] = dest;
	throughDestinations[dest] = port;
	UpdatePins();
	return true;
}

uint8_t Switch::GetThroughDestination(uint8_t port) {
	if(port < Switch::NumPorts) {
		return throughDestinations[port];
	}
	return 0;
}
