#include <Touchstone.hpp>

#include "ff.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

#define LOG_LEVEL	LOG_LEVEL_INFO
#define LOG_MODULE	"Touchstone"
#include "Log.h"

static FIL writeFile;
static bool writeFileOpen = false;

static bool writeFactory = false;

static FIL readFile;
static bool readFileOpen = false;
static uint32_t nextReadPoint;
static char readFileFolder[50];
static char readFileName[50];
static bool write_init_lines = false;
const uint16_t add_comment_limit_per_file = 100;
static uint16_t add_comment_nb = 0;

static bool extract_double_values(const char *line, double *values, uint8_t expected_values) {
	while(expected_values) {
		char *endptr;
		*values = strtod(line, &endptr);
		if(endptr == line) {
			// no number parsed, error
			return false;
		}
		values++;
		expected_values--;
		line = endptr;
	}
	return true;
}

// Returns true if it is a factory file
static bool adjustNames(const char *folder, const char *filename, char *path, char *name) {
	if(strcmp(folder, "FACTORY") != 0) {
		sprintf(path, "0:/%s", folder);
		sprintf(name, "0:%s", filename);
		return false;
	} else {
		sprintf(path, "1:/");
		sprintf(name, "1:/%s", filename);
		return true;
	}
}

static bool open_file(FIL &f, const char *folder, const char *filename, BYTE mode) {
	char name[50];
	char path[50];
	if(adjustNames(folder, filename, path, name)) {
		if(mode & (FA_CREATE_ALWAYS | FA_CREATE_NEW | FA_WRITE)) {
			// write access to factory file requested
			if(!writeFactory) {
				return false;
			}
		}
	}
	auto res = f_chdir(path);
	if(res != FR_OK) {
		// unable to change directory, this coefficient name probably does not exist
		if(mode & FA_CREATE_ALWAYS) {
			// we want to create the file, create the folder first
			if(f_mkdir(path) != FR_OK || f_chdir(path) != FR_OK) {
				return false;
			}
		} else {
			// not creating a new file
			return false;
		}
	}
	res = f_open(&f, name, mode);
	return res == FR_OK;
}

static void closeReadFile() {
	if(readFileOpen) {
		f_close(&readFile);
		readFileOpen = false;
	}
}

uint32_t Touchstone::GetPointNum(const char *folder, const char *filename) {
	FIL f;
	if(!open_file(f, folder, filename, FA_OPEN_EXISTING | FA_READ)) {
		return 0;
	}
	// extract number of ports based on filename ending (s1p or s2p)
	uint8_t ports = filename[strlen(filename) - 2] - '0';
	uint8_t values_per_line = 1 + ports*ports*2;
	uint32_t pointCnt = 0;
	char line[200];
	while(f_gets(line, sizeof(line), &f)) {
		if(line[0] == '!' || line[0] == '#') {
			// ignore comments and option line
			continue;
		}
		double dummy[values_per_line];
		if(extract_double_values(line, dummy, values_per_line)) {
			pointCnt++;
		}
	}
	f_close(&f);
	return pointCnt;
}

bool Touchstone::StartNewFile(const char *folder, const char *filename) {
	if(writeFileOpen) {
		return false;
	}
	if(!open_file(writeFile, folder, filename, FA_CREATE_ALWAYS | FA_WRITE)) {
		return false;
	}
	writeFileOpen = true;
	write_init_lines = false;
	add_comment_nb = 0;

	return true;
}

bool Touchstone::AddComment(const char* comment) {
	if(!writeFileOpen) {
		return false;
	}
	if(write_init_lines == true) {
		// Add comment only allowed just after call of Touchstone::StartNewFile
		return false;
	}
	if(add_comment_nb == add_comment_limit_per_file) {
		// Add comment reached max limit per file allowed
		return false;
	}
	auto res = f_printf(&writeFile, "! %s\r\n", comment);
	if(res < 0) {
		return false;
	}
	add_comment_nb++;
	return true;
}

bool Touchstone::AddPoint(double frequency, double *values, uint8_t num_values) {
	if(!writeFileOpen) {
		return false;
	}
	if(write_init_lines == false) {
		// write initial lines
		auto res = f_printf(&writeFile, "! Automatically created by LibreCAL firmware\r\n");
		if(res < 0) {
			return false;
		}
		// write the option line (only these options are supported)
		res = f_printf(&writeFile, "# GHz S RI R 50.0\r\n");
		if(res < 0) {
			return false;
		}
		write_init_lines = true;
	}
	auto res = f_printf(&writeFile, "%f", frequency);
	if(res < 0) {
		return false;
	}
	while(num_values--) {
		res = f_printf(&writeFile, " %f", *values++);
		if(res < 0) {
			return false;
		}
	}
	res = f_printf(&writeFile, "\r\n");
	if(res < 0) {
		return false;
	}
	return true;
}

bool Touchstone::FinishFile() {
	if(!writeFileOpen) {
		return false;
	}
	f_close(&writeFile);
	writeFileOpen = false;
	return true;
}

int Touchstone::GetPoint(const char *folder, const char *filename,
		uint32_t point, double *values) {
	if(readFileOpen) {
		// check if we are still reading from the same file and requesting a later point
		if(strcmp(folder, readFileFolder) != 0 || strcmp(filename, readFileName) != 0 || nextReadPoint > point) {
			// either reading a different file or already past requested point
			closeReadFile();
		}
	}
	if(!readFileOpen) {
		// needs to open a new file
		if(!open_file(readFile, folder, filename, FA_OPEN_EXISTING | FA_READ)) {
			return 0;
		}
		readFileOpen = true;
		strncpy(readFileFolder, folder, sizeof(readFileFolder));
		strncpy(readFileName, filename, sizeof(readFileName));
		nextReadPoint = 0;
	}
	uint8_t ports = filename[strlen(filename) - 2] - '0';
	uint8_t values_per_line = 1 + ports*ports*2;
	while(nextReadPoint <= point) {
		char line[200];
		if(!f_gets(line, sizeof(line), &readFile)) {
			return 0;
		}
		if(line[0] == '!' || line[0] == '#') {
			// ignore comments and option line
			continue;
		}
		if(extract_double_values(line, values, values_per_line)) {
			nextReadPoint++;
		}
	}
	return values_per_line;
}

void Touchstone::EnableFactoryWriting() {
	writeFactory = true;
}

bool Touchstone::DeleteFile(const char *folder, const char *filename) {
	char name[50];
	char path[50];
	if(adjustNames(folder, filename, path, name)) {
		// delete access to factory file requested
		if(!writeFactory) {
			return false;
		}
	}
	if(f_chdir(path) != FR_OK) {
		return false;
	}
	if(f_unlink(name) != FR_OK) {
		return false;
	}
	// check if directory is empty now
	DIR dir;
	FILINFO fno;
	if(f_opendir(&dir, path) != FR_OK) {
		// this is technically an error, but we already deleted the file, so return true.
		// Only negative effect is that the directory of the file may still be existing
		// even if empty
		return true;
	}
	if(f_readdir(&dir, &fno) != FR_OK) {
		// same as above
		f_closedir(&dir);
		return true;
	}
	f_closedir(&dir);
	if(fno.fname[0] == 0) {
		// complete directory is empty, delete the folder
		f_chdir("0:/");
		f_unlink(path);
	}
	return true;
}

bool Touchstone::GetUserCoefficientName(uint8_t index, char *name, uint16_t maxlen) {
	DIR dir;
	FILINFO fno;
	if(f_opendir(&dir, "0:/") != FR_OK) {
		return false;
	}
	while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
		if(fno.fattrib & AM_DIR) {
			if(strcmp(fno.fname, "System Volume Information") == 0
					|| strncmp(fno.fname, ".Trash", 6) == 0) {
				// skip directories that were created by linux/windows on the drive
				continue;
			}
			// is a directory
			if(index == 0) {
				// reached the requested directory
				snprintf(name, maxlen, fno.fname);
				f_closedir(&dir);
				return true;
			} else {
				index--;
			}
		}
	}
	f_closedir(&dir);
	return false;
}

bool Touchstone::PrintFile(const char *folder, const char *filename, SCPI::scpi_tx_callback tx_func, uint8_t interface) {
	if(readFileOpen) {
		closeReadFile();
	}
	tx_func((uint8_t*) "START\r\n", 7, interface);
	if(!open_file(readFile, folder, filename, FA_OPEN_EXISTING | FA_READ)) {
		return false;
	}
	uint8_t buffer[256];
	while(true) {
		UINT br;
		if(f_read(&readFile, buffer, sizeof(buffer), &br) != FR_OK) {
			f_close(&readFile);
			return false;
		}
		if(br > 0) {
			tx_func(buffer, br, interface);
		}
		if(br < sizeof(buffer)) {
			break;
		}
	}
	f_close(&readFile);
	tx_func((uint8_t*) "END\r\n", 5, interface);
	return true;
}

// Implemented in main.cpp
bool createInfoFile();
extern FATFS fs1;

bool Touchstone::clearFactory() {
	if(!writeFactory) {
		LOG_ERR("Factory deletion not allowed");
		return false;
	}
    
    // close any possibly still open file
    if(readFileOpen) {
		closeReadFile();
	}
    FinishFile();

	// format the factory drive
	BYTE work[FF_MAX_SS];
	FRESULT status;
	if((status = f_mkfs("1:", 0, work, sizeof(work))) != FR_OK) {
		LOG_ERR("mkfs failed: %d", status);
		return false;
	}
	if((status = f_mount(&fs1, "1:", 1)) != FR_OK) {
		LOG_ERR("mount failed: %d", status);
		return false;
	}
	if((status = f_setlabel("1:LibreCAL_R")) != FR_OK) {
		LOG_ERR("set label failed: %d", status);
		return false;
	}


    // needs to recreate the information file
    return createInfoFile();
}
