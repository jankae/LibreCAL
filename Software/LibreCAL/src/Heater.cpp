#include "Heater.hpp"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"

#include "FreeRTOS.h"
#include "task.h"

#include <cmath>

static uint8_t target;
static bool stable = false;
static float temp;
static float power;

using namespace Heater;

static void setPWM(uint16_t power) {
	uint slice = pwm_gpio_to_slice_num(PWMPin);
	uint channel = pwm_gpio_to_channel(PWMPin);
	pwm_set_chan_level(slice, channel, power);
}

void HeaterTask(void*) {
	// Controller parameters
	constexpr float assumed_ambient = 22.0f; // in °C
	constexpr float thermal_resistance = 30.0f; // in °C/W
	constexpr float P = 0.4f; // in W/delta°C
	constexpr float I = 0.05f; // in W/delta°C/s
	constexpr float I_limit = 2.0f;
	constexpr float ControllerPeriod = 0.05;

	// Stable check paramters
	constexpr float allowedDeviation = 0.25f;
	constexpr uint16_t minStableTime = 60000;

	static constexpr float alpha = 0.95;
	float adc_avg = adc_read();
	float integral = 0.0f;

	uint32_t lastUnstableTime = xTaskGetTickCount();

	while(1) {

		adc_avg = adc_read() * (1.0f - alpha) + adc_avg * alpha;
		// Reference resistor is 10k, NTC has additional 100 Ohm in series
		float NTC_resistance = adc_avg / (4096 - adc_avg) * 10000 - 100;
		constexpr float NTC_nominal = 10000.0f;
		constexpr float NTC_B = 4300.0f;
		constexpr float T0 = 25.0f;
		constexpr float Tzero = 273.15f;
		temp = (T0 + Tzero) * NTC_B / ((T0 + Tzero) * logf(NTC_resistance / NTC_nominal) + NTC_B) - Tzero;

		float basePower = (temp - assumed_ambient) / thermal_resistance; // required power to hold current temperature
		float deviation = target - temp;
		if(power < maxPower || power > 0) {
			// only update integral term when not already at control limit
			integral += deviation * I * ControllerPeriod;
		}
		// anti windup
		if(integral > I_limit) {
			integral = I_limit;
		} else if(integral < -I_limit) {
			integral = -I_limit;
		}
		float p = deviation * P + integral;
		if(p > maxPower) {
			p = maxPower;
		} else if(p < 0) {
			p = 0;
		}
		power = p;

		// convert power to PWM, maximum power is approximately 1.9W
		int32_t pwm = power * UINT16_MAX / maxPower;
		if(pwm > UINT16_MAX) {
			pwm = UINT16_MAX;
		} else if(pwm < 0) {
			pwm = 0;
		}
		setPWM(pwm);

		// check if temperature is stable
		if(fabs(temp - target) > allowedDeviation) {
			stable = false;
			lastUnstableTime = xTaskGetTickCount();
		} else if(xTaskGetTickCount() - lastUnstableTime > minStableTime) {
			stable = true;
		}

		vTaskDelay(ControllerPeriod * 1000);
	}
}

void Heater::Init() {
	gpio_set_function(PWMPin, GPIO_FUNC_PWM);
	uint slice = pwm_gpio_to_slice_num(PWMPin);
	uint channel = pwm_gpio_to_channel(PWMPin);
	pwm_set_clkdiv(slice, 95.38f); // for 20Hz PWM
	pwm_set_wrap(slice, UINT16_MAX);
	setPWM(0);
	pwm_set_enabled(slice, true);

	adc_init();
	adc_gpio_init(ADCPin);
	adc_select_input(ADCPin - 26);

	xTaskCreate(HeaterTask, "Heater", 512, NULL, 3, NULL);
}

void Heater::SetTarget(uint8_t celsius) {
	target = celsius;
}

float Heater::GetTemp() {
	return temp;
}

bool Heater::IsStable() {
	return stable;
}

float Heater::GetPower() {
	return power;
}
