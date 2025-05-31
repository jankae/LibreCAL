/*
 * Flash.hpp
 *
 *  Created on: Aug 26, 2020
 *      Author: jan
 */

#ifndef DRIVERS_FLASH_HPP_
#define DRIVERS_FLASH_HPP_

#include "FreeRTOS.h"
#include "semphr.h"

#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/time.h"

class Flash {
public:
	Flash(spi_inst_t *spi, uint8_t SCLK_pin, uint8_t MOSI_pin, uint8_t MISO_pin, uint8_t CS_pin)
	: spi(spi),SCLK_pin(SCLK_pin),MOSI_pin(MOSI_pin),MISO_pin(MISO_pin),CS_pin(CS_pin){
		mutex = xSemaphoreCreateMutex();
		totalSize = 0;
	};

	bool isPresent();
	void read(uint32_t address, uint16_t length, void *dest);
	bool write(uint32_t address, uint16_t length, const uint8_t *src);
	bool eraseChip();
	bool eraseSector(uint32_t address);
	bool erase32Block(uint32_t address);
	bool erase64Block(uint32_t address);
	bool eraseRange(uint32_t start, uint32_t len);
	uint32_t size();
	const spi_inst_t* const getSpi() const {
		return spi;
	}
	static constexpr uint32_t PageSize = 256;
	static constexpr uint32_t SectorSize = 4096;
	static constexpr uint32_t Block32Size = 32768;
	static constexpr uint32_t Block64Size = 65536;

private:
	void CS(bool high) {
		gpio_put(CS_pin, high);
		if(high) {
			sleep_us(1);
		}
	}
	// Starts the reading process without actually reading any bytes
	void initiateRead(uint32_t address);
	void EnableWrite();
	bool WaitBusy(uint32_t timeout);
	spi_inst_t * const spi;
	const uint8_t CS_pin, MISO_pin, MOSI_pin, SCLK_pin;
	SemaphoreHandle_t mutex;
	uint32_t totalSize;
};


#endif /* DRIVERS_FLASH_HPP_ */
