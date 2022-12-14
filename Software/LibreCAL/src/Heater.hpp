/*
 * Heater.hpp
 *
 *  Created on: 1 Sep 2022
 *      Author: jan
 */

#ifndef HEATER_HPP_
#define HEATER_HPP_

#include <cstdint>

namespace Heater {

static constexpr uint8_t PWMPin = 28;
static constexpr uint8_t ADCPin = 29;

static constexpr float maxPower = 1.9f;

void Init();
void SetTarget(uint8_t celsius);
float GetTemp();
bool IsStable();
float GetPower();

};

#endif /* HEATER_HPP_ */
