#pragma once

#include <stdint.h>

#include "Flash.hpp"

extern Flash flash;

#define FF_FLASH_DISKS			2
// DISK0 is writable
#define FF_FLASH_DISK1_SIZE		1048576
#define FF_FLASH_DISK0_SIZE		(flash.size() - FF_FLASH_DISK1_SIZE)

#define FF_FLASH_DISK0_SECTORS	(FF_FLASH_DISK0_SIZE / Flash::SectorSize)
#define FF_FLASH_DISK1_SECTORS	(FF_FLASH_DISK1_SIZE / Flash::SectorSize)

