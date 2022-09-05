#include <Touchstone.hpp>

#include "ff.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

static FIL writeFile;
static bool writeFileOpen = false;

static FIL readFile;
static bool readFileOpen = false;
static uint32_t nextReadPoint;
static char readFileFolder[50];
static char readFileName[50];

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

static bool open_file(FIL &f, const char *folder, const char *filename, BYTE mode) {
	char name[50];
	if(folder) {
		snprintf(name, sizeof(name), "0:%s", filename);
		char dir[50];
		snprintf(dir, sizeof(dir), "0:/%s", folder);
		auto res = f_chdir(dir);
		if(res != FR_OK) {
			// unable to change directory, this coefficient name probably does not exist
			if(mode & FA_CREATE_ALWAYS) {
				// we want to create the file, create the folder first
				if(f_mkdir(dir) != FR_OK || f_chdir(dir) != FR_OK) {
					return false;
				}
			} else {
				// not creating a new file
				return false;
			}
		}
	} else {
		snprintf(name, sizeof(name), "1:/%s", filename);
	}
	auto res = f_open(&f, name, mode);
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
	return true;
}

bool Touchstone::AddPoint(double frequency, double *values, uint8_t num_values) {
	if(!writeFileOpen) {
		return false;
	}
	f_printf(&writeFile, "%f", frequency);
	while(num_values--) {
		f_printf(&writeFile, " %f", *values++);
	}
	f_printf(&writeFile, "\r\n");
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
