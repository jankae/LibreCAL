/*
 * Switch.hpp
 *
 *  Created on: 1 Sep 2022
 *      Author: jan
 */

#ifndef SWITCH_HPP_
#define SWITCH_HPP_

#include <cstdint>

namespace Switch {

static constexpr uint8_t NumPorts = 4;

enum class Standard {
	Open,
	Short,
	Load,
	Through,
	None,
};

void Init();
void SetStandard(uint8_t port, Standard s);
bool SetThrough(uint8_t port, uint8_t dest);
Standard GetStandard(uint8_t port);
uint8_t GetThroughDestination(uint8_t port);

const char* StandardName(Standard s);
bool NameMatched(const char *name, Standard s);

};


#endif /* SWITCH_HPP_ */
