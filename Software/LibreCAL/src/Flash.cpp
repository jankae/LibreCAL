#include "Flash.hpp"

#include "FreeRTOS.h"
#include "task.h"
#include <cstring>

#include <stdio.h>

#define LOG_LEVEL	LOG_LEVEL_INFO
#define LOG_MODULE	"Flash"
#include "Log.h"

bool Flash::isPresent() {
	xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
	CS(false);
	// read JEDEC ID
	uint8_t send[4] = {0x9F};
	uint8_t recv[4];
	spi_write_read_blocking(spi, send, recv, 4);
	CS(true);
	// Check against valid manufacturer IDs
	constexpr uint8_t valid_ids[] = {0xEF, 0x68};
	bool valid = false;
	for (uint8_t i = 0; i < sizeof(valid_ids); i++) {
		if (recv[1] == valid_ids[i]) {
			valid = true;
			totalSize = 0;
			switch(recv[3]) {
			case 0x18: totalSize = 16777216; break;
			case 0x17: totalSize = 8388608; break;
			case 0x16: totalSize = 4194304; break;
			case 0x15: totalSize = 2097152; break;
			}
			break;
		}
	}
	xSemaphoreGiveRecursive(mutex);
	return valid;
}

void Flash::read(uint32_t address, uint16_t length, void *dest) {
	xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
	initiateRead(address);
	// read data
	spi_read_blocking(spi, 0x00, (uint8_t*) dest, length);
	CS(true);
	xSemaphoreGiveRecursive(mutex);
}

bool Flash::write(uint32_t address, uint16_t length, const uint8_t *src) {
	xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
	if(address % PageSize != 0 || length%PageSize != 0) {
		// only writes to complete pages allowed
		LOG_ERR("Invalid write address/size: 0x%08x/%u", address, length);
		xSemaphoreGiveRecursive(mutex);
		return false;
	}
	address &= 0x00FFFFFF;
	LOG_DEBUG("Writing %u bytes to address 0x%08x", length, address);
	while(length > 0) {
		EnableWrite();
		CS(false);
		uint8_t cmd[4] = {
			0x02,
			(uint8_t) ((address >> 16) & 0xFF),
			(uint8_t) ((address >> 8) & 0xFF),
			(uint8_t) (address & 0xFF),
		};
		// issue write command
		spi_write_blocking(spi, cmd, 4);
		// write data
		spi_write_blocking(spi, (uint8_t*) src, 256);
		CS(true);
		if(!WaitBusy(20)) {
			LOG_ERR("Write timed out");
			xSemaphoreGiveRecursive(mutex);
			return false;
		}
		// Verify
		uint8_t buf[256];
		read(address, 256, buf);
		if(memcmp(src, buf, 256)) {
			LOG_ERR("Verification error");
			xSemaphoreGiveRecursive(mutex);
			return false;
		}
		address += 256;
		length -= 256;
		src += 256;
	}
	xSemaphoreGiveRecursive(mutex);
	return true;
}

void Flash::EnableWrite() {
	CS(false);
	// enable write latch
	uint8_t wel = 0x06;
	spi_write_blocking(spi, &wel, 1);
	CS(true);
}

bool Flash::eraseChip() {
	xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
	LOG_INFO("Erasing chip...");
	EnableWrite();
	CS(false);
	uint8_t chip_erase = 0x60;
	spi_write_blocking(spi, &chip_erase, 1);
	CS(true);
	return WaitBusy(25000);
}

bool Flash::eraseSector(uint32_t address) {
	xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
	// align address with sector address
	address -= address % SectorSize;
	LOG_INFO("Erasing sector at 0x%08x", address);
	EnableWrite();
	CS(false);
	uint8_t cmd[4] = {
		0x20,
		(uint8_t) ((address >> 16) & 0xFF),
		(uint8_t) ((address >> 8) & 0xFF),
		(uint8_t) (address & 0xFF),
	};
	spi_write_blocking(spi, cmd, 4);
	CS(true);
	bool ret =  WaitBusy(25000);
	xSemaphoreGiveRecursive(mutex);
	return ret;
}

bool Flash::erase32Block(uint32_t address) {
	xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
	// align address with block address
	address -= address % Block32Size;
	LOG_INFO("Erasing 32kB block at 0x%08x", address);
	EnableWrite();
	CS(false);
	uint8_t cmd[4] = {
		0x52,
		(uint8_t) ((address >> 16) & 0xFF),
		(uint8_t) ((address >> 8) & 0xFF),
		(uint8_t) (address & 0xFF),
	};
	spi_write_blocking(spi, cmd, 4);
	CS(true);
	bool ret =  WaitBusy(25000);
	xSemaphoreGiveRecursive(mutex);
	return ret;
}

bool Flash::erase64Block(uint32_t address) {
	xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
	// align address with block address
	address -= address % Block64Size;
	LOG_INFO("Erasing 64kB block at 0x%08x", address);
	EnableWrite();
	CS(false);
	uint8_t cmd[4] = {
		0xD8,
		(uint8_t) ((address >> 16) & 0xFF),
		(uint8_t) ((address >> 8) & 0xFF),
		(uint8_t) (address & 0xFF),
	};
	spi_write_blocking(spi, cmd, 4);
	CS(true);
	bool ret =  WaitBusy(25000);
	xSemaphoreGiveRecursive(mutex);
	return ret;
}

void Flash::initiateRead(uint32_t address) {
	address &= 0x00FFFFFF;
	CS(false);
	uint8_t cmd[4] = {
		0x03,
		(uint8_t) ((address >> 16) & 0xFF),
		(uint8_t) ((address >> 8) & 0xFF),
		(uint8_t) (address & 0xFF),
	};
	// issue read command
	spi_write_blocking(spi, cmd, 4);
}

bool Flash::WaitBusy(uint32_t timeout) {
	uint32_t starttime = xTaskGetTickCount();
	CS(false);
	uint8_t readStatus1 = 0x05;
	spi_write_blocking(spi, &readStatus1, 1);
	do {
		vTaskDelay(1);
		uint8_t status1;
		spi_read_blocking(spi, 0x00, &status1, 1);
		if (!(status1 & 0x01)) {
			CS(true);
			return true;
		}
	} while (xTaskGetTickCount() - starttime < timeout);
	// timed out
	CS(true);
	LOG_ERR("Timeout occurred");
	return false;
}

bool Flash::eraseRange(uint32_t start, uint32_t len) {
	xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
	if(start % SectorSize != 0) {
		LOG_ERR("Start address of range has to be sector aligned (is %lu)", start);
		xSemaphoreGiveRecursive(mutex);
		return false;
	}
	if(len % SectorSize != 0) {
		LOG_ERR("Length of range has to be multiple of sector size (is %lu)", len);
		xSemaphoreGiveRecursive(mutex);
		return false;
	}
	uint32_t erased_len = 0;
	while(erased_len < len) {
		uint32_t remaining = len - erased_len;
		if(remaining >= Block64Size && start % Block64Size == 0) {
			erase64Block(start);
			erased_len += Block64Size;
			start += Block64Size;
			continue;
		}
		if(remaining >= Block32Size && start % Block32Size == 0) {
			erase32Block(start);
			erased_len += Block32Size;
			start += Block32Size;
			continue;
		}
		if(remaining >= SectorSize && start % SectorSize == 0) {
			eraseSector(start);
			erased_len += SectorSize;
			start += SectorSize;
			continue;
		}
		// Should never get here
	}
	xSemaphoreGiveRecursive(mutex);
	return true;
}

uint32_t Flash::size() {
	return totalSize;
}
