#include "UserInterface.hpp"

#include "Switch.hpp"
#include "Heater.hpp"
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdlib.h"
#include <cstring>
#include "Log.h"
#include "main.h"

namespace UserInterface {

enum class LED : uint8_t {
	READY = 0,
	WAIT = 1,
	PORT1 = 2,
	PORT2 = 3,
	PORT3 = 4,
	PORT4 = 5,
	OPEN = 6,
	SHORT = 7,
	LOAD = 8,
	THROUGH = 9,
};

constexpr uint8_t LEDpins[] = {5,6,7,21,8,22,23,24,25,26};

enum class Button : uint8_t {
	PORT = 0,
	FUNCTION = 1,
};

constexpr uint8_t Buttonpins[] = {4,27};

constexpr uint32_t editTime = 3000;
constexpr uint32_t ledBlinkPeriod = 200;
constexpr uint32_t ledBlinkOnTime = 100;

void setLED(LED led, bool on) {
#ifdef ENABLE_UART
	if(LEDpins[(int) led] == LOG_UART_PIN) {
		return;
	}
#endif
	gpio_put(LEDpins[(int) led], !on);
}

bool IsFunctionHeld() {
	return !gpio_get(Buttonpins[(int)Button::FUNCTION]);
}

void Task(void*) {
	bool editing = false;
	uint8_t selectedPort = 0;
	uint8_t buttonHistory[sizeof(Buttonpins)];
	memset(buttonHistory, 0, sizeof(buttonHistory));
	TickType_t lastButtonPress;
	while(1) {
		// update button status
		bool buttonClicked[sizeof(Buttonpins)];
		for(uint8_t i=0;i<sizeof(Buttonpins);i++) {
			bool pressed = !gpio_get(Buttonpins[i]);
			buttonHistory[i] = buttonHistory[i] << 1 | (pressed ? 0x01 : 0x00);
			if(buttonHistory[i] == 0xFE) {
				// button has just been released
				buttonClicked[i] = true;
			} else {
				buttonClicked[i] = false;
			}
		}
		if(editing && xTaskGetTickCount() - lastButtonPress > editTime) {
			editing = false;
		}
		// Handle button presses
		if(buttonClicked[(int) Button::PORT]) {
			lastButtonPress = xTaskGetTickCount();
			if(!editing) {
				// start editing process
				editing = true;
			} else {
				// already editing, change port
				selectedPort++;
				if(selectedPort >= Switch::NumPorts) {
					selectedPort = 0;
				}
			}
		}
		if(editing && buttonClicked[(int) Button::FUNCTION]) {
			lastButtonPress = xTaskGetTickCount();
			auto state = Switch::GetStandard(selectedPort);
			// Select next state
			switch(state) {
			case Switch::Standard::Open: state = Switch::Standard::Short; break;
			case Switch::Standard::Short: state = Switch::Standard::Load; break;
			case Switch::Standard::Load: state = Switch::Standard::Through; break;
			case Switch::Standard::Through: state = Switch::Standard::None; break;
			case Switch::Standard::None: state = Switch::Standard::Open; break;
			}
			Switch::SetStandard(selectedPort, state);
		}

		// update LEDs

		// wait/ready are solid on when in default mode and blink when in any other mode
		bool on = (getUsbMode() == MODE_DEFAULT) || (xTaskGetTickCount() % ledBlinkPeriod > ledBlinkOnTime);
		if(Heater::IsStable()) {
			setLED(LED::WAIT, false);
			setLED(LED::READY, on);
		} else {
			setLED(LED::WAIT, on);
			setLED(LED::READY, false);
		}
		if(editing) {
			auto state = Switch::GetStandard(selectedPort);
			setLED(LED::OPEN, state == Switch::Standard::Open);
			setLED(LED::SHORT, state == Switch::Standard::Short);
			setLED(LED::LOAD, state == Switch::Standard::Load);
			setLED(LED::THROUGH, state == Switch::Standard::Through);
			// blink LED of selected port
			setLED(LED::PORT1, selectedPort == 0 && xTaskGetTickCount() % ledBlinkPeriod > ledBlinkOnTime);
			setLED(LED::PORT2, selectedPort == 1 && xTaskGetTickCount() % ledBlinkPeriod > ledBlinkOnTime);
			setLED(LED::PORT3, selectedPort == 2 && xTaskGetTickCount() % ledBlinkPeriod > ledBlinkOnTime);
			setLED(LED::PORT4, selectedPort == 3 && xTaskGetTickCount() % ledBlinkPeriod > ledBlinkOnTime);
		} else {
			// - enable LED for every port which has any standard defined
			// - enable LED for standard which is used at any port
			// Turn all standard LEDs off first (easier handling, and not visible due to the short duration
			setLED(LED::OPEN, false);
			setLED(LED::SHORT, false);
			setLED(LED::LOAD, false);
			setLED(LED::THROUGH, false);
			for(int i=0;i<Switch::NumPorts;i++) {
				auto state = Switch::GetStandard(i);
				setLED((LED)((int) LED::PORT1 + i), state != Switch::Standard::None);
				switch(state) {
				case Switch::Standard::Open: setLED(LED::OPEN, true); break;
				case Switch::Standard::Short: setLED(LED::SHORT, true); break;
				case Switch::Standard::Load: setLED(LED::LOAD, true); break;
				case Switch::Standard::Through: setLED(LED::THROUGH, true); break;
				}
			}
		}
		vTaskDelay(10);
	}
}

void Init() {
	// Initialize pins
	for(uint8_t i=0;i<sizeof(LEDpins);i++) {
#ifdef ENABLE_UART
		if(LEDpins[i] == LOG_UART_PIN) {
			continue;
		}
#endif
		gpio_init(LEDpins[i]);
	    gpio_set_dir(LEDpins[i], GPIO_OUT);
	    gpio_put(LEDpins[i], true);
	}
	for(uint8_t i=0;i<sizeof(Buttonpins);i++) {
		gpio_init(Buttonpins[i]);
	    gpio_set_dir(Buttonpins[i], GPIO_IN);
	    gpio_set_pulls(Buttonpins[i], true, false);
	}
	xTaskCreate(Task, "UserInterface", 512, NULL, 3, NULL);
}

};
