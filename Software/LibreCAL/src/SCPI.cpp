#include "SCPI.hpp"

#include "serial.h"

#include <cstring>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <array>

#include "Heater.hpp"
#include "Switch.hpp"
#include "Touchstone.hpp"

#include <pico/bootrom.h>
#include "hardware/rtc.h"

#include "FreeRTOS.h"
#include "task.h"

using namespace SCPI;

constexpr int CommentMaxSize = 120;
constexpr int ParseLastCmdMaxSize = CommentMaxSize+1;
constexpr int ParseArgumentsMax = CommentMaxSize+1;

constexpr int NumInterfaces = 2;
constexpr int BufferSize = 256;

static char buffer[NumInterfaces][BufferSize];
static uint16_t rx_cnt[NumInterfaces];

static scpi_tx_callback tx_data;

static char scpi_date_time_utc[] = "UTC+00:00"; // Default UTC+00:00 shall be set by SCPI :DATE_TIME

static void tx_string(const char *s, uint8_t interface) {
	tx_data((const uint8_t*) s, strlen(s), interface);
}

static void tx_int(int i, uint8_t interface) {
	char s[20];
	snprintf(s, sizeof(s), "%d", i);
	tx_string(s, interface);
}

static void tx_double(double d, uint8_t interface) {
	char s[20];
	snprintf(s, sizeof(s), "%f", d);
	tx_string(s, interface);
}

static bool validate_date(const char *date, datetime_t *t) {
	// Ensure the date has the correct length and format
	if (strlen(date) != 10) {
		return false;
	}
	// Parse the year, month, and day
	const char* p = date;
	t->year = 0;
	while (isdigit(*p)) {
		t->year = (t->year * 10) + (*p++ - '0');
	}
	if (*p++ != '/') {
		return false;
	}
	t->month = 0;
	while (isdigit(*p)) {
		t->month = (t->month * 10) + (*p++ - '0');
	}
	if (*p++ != '/') {
		return false;
	}
	t->day = 0;
	while (isdigit(*p)) {
		t->day = (t->day * 10) + (*p++ - '0');
	}
	// Ensure the parsed values are valid
	if (t->year < 0 || t->month < 1 || t->month > 12 || t->day < 1 || t->day > 31) {
		return false;
	}
	return true;
}

static bool validate_time(const char *time, datetime_t *t) {
	// Ensure the time has the correct length and format
	if (strlen(time) != 8) {
		return false;
	}
	// Parse the hour, min, and sec
	const char* p = time;
	t->hour = 0;
	while (isdigit(*p)) {
		t->hour = (t->hour * 10) + (*p++ - '0');
	}
	if (*p++ != ':') {
		return false;
	}
	t->min = 0;
	while (isdigit(*p)) {
		t->min = (t->min * 10) + (*p++ - '0');
	}
	if (*p++ != ':') {
		return false;
	}
	t->sec = 0;
	while (isdigit(*p)) {
		t->sec = (t->sec * 10) + (*p++ - '0');
	}
	// Ensure the parsed values are valid
	if (t->hour < 0 || t->hour > 23 || t->min <  0 || t->min > 59  || t->sec <  0 || t->sec > 59) {
		return false;
	}
	return true;
}

static bool validate_utc(const char *utc) {
	// Ensure the UTC has the correct length and format
	// Example UTC correct format "UTC−10:00", "UTC+12:00"...
	// See https://en.wikipedia.org/wiki/List_of_UTC_offsets
	if (strlen(utc) != 9) {
		return false;
	}
	if (strncmp(utc, "UTC", 3) != 0) {
		return false;
	}
	// valid UTC offset in the format of "+/-HH:MM"
	const char* p = &utc[3];
	// Check the sign of the offset
	if (*p == '+') {
		p++;
	} else if (*p == '-') {
		p++;
	} else {
		return false;
	}
	// Parse the hour and min
	int hour = 0;
	while (isdigit(*p)) {
		hour = (hour * 10) + (*p++ - '0');
	}
	if (*p++ != ':') {
		return false;
	}
	int min = 0;
	while (isdigit(*p)) {
		min = (min * 10) + (*p++ - '0');
	}
	// Ensure the parsed values are valid
	if (hour < 0 || hour > 14 || min <  0 || min > 59) {
		return false;
	}
	return true;
}

class Command {
public:
	using Cmd = void(*)(char *argv[], int argc, int interface);
	using Query = void(*)(char *argv[], int argc, int interface);

	Command(const char *name, Cmd cmd = nullptr, Query query = nullptr, int min_args_cmd = 0, int min_args_query = 0) :
		name(name), cmd(cmd), query(query), min_args_cmd(min_args_cmd), min_args_query(min_args_query) {}

	bool parse(char *argv[], int argc, uint8_t interface) const {
		if(argv[0][0] == ':') {
			// remove possible leading colon
			argv[0]++;
		}
		// check if command matches name
		auto cmd_len = strlen(argv[0]);
		auto name_len = strlen(name);
		bool isMatch = true;
		uint8_t nameOffset = 0;
		int i=0;
		for(;i + nameOffset<name_len;i++) {
			char carg = argv[0][i];
			char cname = name[i+nameOffset];
			if(toupper(carg) != toupper(cname)) {
				// Mismatch. There are a couple of cases where this is allowed, check them
				if(islower(cname) && carg == ':') {
					// check if name contains a next column. If so, the current branch was abbreviated
					auto nextcolon = strchr(&name[i+nameOffset], ':');
					if(!nextcolon) {
						isMatch = false;
						break;
					}
					// adjust nameOffset and continue
					nameOffset += nextcolon - &name[i+nameOffset];
					continue;
				} else if(islower(cname) && (carg == '?' || carg == '\0')) {
					// check if the leaf name was abbreviated
					auto nextcolon = strchr(&name[i+nameOffset], ':');
					if(nextcolon) {
						// not a leaf, no match
						isMatch = false;
						break;
					} else {
						nameOffset = name_len - i;
						break;
					}
				} else {
					// an actual mismatch
					isMatch = false;
					break;
				}
			}
		}
		if(!isMatch) {
			// already found a mismatch during string comparison
			return false;
		} else if(i + nameOffset != name_len) {
			// strings are a match, but argv[0] was shorter than name
			return false;
		}
		// got a match check cmd/query
		if(argv[0][i] == '\0' && cmd != nullptr) {
			if(argc - 1 < min_args_cmd) {
				return false;
			}
			cmd(argv, argc, interface);
			return true;
		} else if(argv[0][i] == '?' && query != nullptr) {
			if(argc - 1 < min_args_query) {
				return false;
			}
			query(argv, argc, interface);
			return true;
		}
		// cmd or query not available
		return false;
	}

	const char *name;
	Cmd cmd;
	Query query;
	int min_args_cmd, min_args_query;
};

// can't be declared inside of commands as it needs the complete command list
static void scpi_lst(char *argv[], int argc, int interface);

#define ARRAY_SIZE(d) (sizeof(d)/sizeof(d[0]))

static bool arg_to_int(const char* arg, int &i)
{
	char *endptr;
	i = strtol(arg, &endptr, 10);
	if(*endptr == '\0') {
		// successful conversion of the complete argument
		return true;
	} else {
		return false;
	}
}

static const char* coefficientOptionEnding(const char *option) {
	const char *s1p[] = {
			"P1_OPEN",
			"P1_SHORT",
			"P1_LOAD",
			"P2_OPEN",
			"P2_SHORT",
			"P2_LOAD",
			"P3_OPEN",
			"P3_SHORT",
			"P3_LOAD",
			"P4_OPEN",
			"P4_SHORT",
			"P4_LOAD",
	};
	const char *s2p[] = {
			"P12_THROUGH",
			"P13_THROUGH",
			"P14_THROUGH",
			"P23_THROUGH",
			"P24_THROUGH",
			"P34_THROUGH",
	};
	for(uint8_t i=0;i<ARRAY_SIZE(s1p);i++) {
		if(strcmp(option, s1p[i]) == 0) {
			return "s1p";
		}
	}
	for(uint8_t i=0;i<ARRAY_SIZE(s2p);i++) {
		if(strcmp(option, s2p[i]) == 0) {
			return "s2p";
		}
	}
	return nullptr;
}

static const Command commands[] = {
		Command("*IDN", nullptr,
		[](char *argv[], int argc, int interface){
			tx_string("LibreCAL,LibreCAL,", interface);
			tx_string(getSerial(), interface);
			char resp[20];
			snprintf(resp, sizeof(resp), ",%d.%d.%d\r\n", FW_MAJOR, FW_MINOR, FW_PATCH);
			tx_string(resp, interface);
		}),
		Command("*LST", nullptr, scpi_lst),
		Command("FIRMWARE", nullptr, [](char *argv[], int argc, int interface){
			char resp[20];
			snprintf(resp, sizeof(resp), "%d.%d.%d\r\n", FW_MAJOR, FW_MINOR, FW_PATCH);
			tx_string(resp, interface);
		}),
		Command("PORTS", nullptr, [](char *argv[], int argc, int interface){
			tx_string("4\r\n", interface);
		}),
		Command("TEMPerature", [](char *argv[], int argc, int interface){
			int target;
			if(!arg_to_int(argv[1], target)) {
				tx_string("ERROR\r\n", interface);
			} else {
				Heater::SetTarget(target);
				tx_string("\r\n", interface);
			}
		},
		[](char *argv[], int argc, int interface){
			char resp[20];
			snprintf(resp, sizeof(resp), "%f\r\n", Heater::GetTemp());
			tx_string(resp, interface);
		}, 1),
		Command("TEMPerature:STABLE", nullptr,
		[](char *argv[], int argc, int interface){
			if(Heater::IsStable()) {
				tx_string("TRUE\r\n", interface);
			} else {
				tx_string("FALSE\r\n", interface);
			}
		}),
		Command("HEATer:POWer", nullptr, [](char *argv[], int argc, int interface){
			char resp[20];
			snprintf(resp, sizeof(resp), "%f\r\n", Heater::GetPower());
			tx_string(resp, interface);
		}),
		Command("PORT", [](char *argv[], int argc, int interface){
			int port;
			if(!arg_to_int(argv[1], port)) {
				tx_string("ERROR\r\n", interface);
				return;
			}
			if(port < 1 || port > Switch::NumPorts) {
				tx_string("ERROR\r\n", interface);
				return;
			}
			std::array<Switch::Standard, 5> standards = {Switch::Standard::Open, Switch::Standard::Short, Switch::Standard::Load, Switch::Standard::Through, Switch::Standard::None};
			for(auto s : standards) {
				if(Switch::NameMatched(argv[2], s)) {
					// found the correct standard
					if(s == Switch::Standard::Through) {
						// also needs the destination port
						int dest;
						if(argc < 4 || !arg_to_int(argv[3], dest) || !Switch::SetThrough(port - 1, dest - 1)) {
							// either no/invalid argument or failed to set standard
							break;
						}
					} else {
						Switch::SetStandard(port - 1, s);
					}
					tx_string("\r\n", interface);
					return;
				}
			}
			// no standard with the given name found
			tx_string("ERROR\r\n", interface);
		},
		[](char *argv[], int argc, int interface){
			int port;
			if(!arg_to_int(argv[1], port)) {
				tx_string("ERROR\r\n", interface);
				return;
			}
			if(port < 1 || port > Switch::NumPorts) {
				tx_string("ERROR\r\n", interface);
				return;
			}
			auto standard = Switch::GetStandard(port - 1);
			tx_string(Switch::StandardName(standard), interface);
			if(standard == Switch::Standard::Through) {
				// also print the destination port
				tx_string(" ", interface);
				tx_int(Switch::GetThroughDestination(port - 1) + 1, interface);
			}
			tx_string("\r\n", interface);
		}, 2, 1),
		Command("COEFFicient:LIST", nullptr, [](char *argv[], int argc, int interface){
			tx_string("FACTORY", interface);
			uint8_t i=0;
			char name[50];
			while(Touchstone::GetUserCoefficientName(i, name, sizeof(name))) {
				tx_string(",", interface);
				tx_string(name, interface);
				i++;
			}
			tx_string("\r\n", interface);
		}),
		Command("COEFFicient:CREATE", [](char *argv[], int argc, int interface){
			if(!coefficientOptionEnding(argv[2])) {
				// invalid coefficient name
				tx_string("ERROR\r\n", interface);
				return;
			}
			char filename[50];
			snprintf(filename, sizeof(filename), "%s.%s", argv[2], coefficientOptionEnding(argv[2]));
			if(!Touchstone::StartNewFile(argv[1], filename)) {
				// failed to create file
				tx_string("ERROR\r\n", interface);
				return;
			}
			// file started
			tx_string("\r\n", interface);
		}, nullptr, 2),
		Command("COEFFicient:DELete", [](char *argv[], int argc, int interface){
			if(!coefficientOptionEnding(argv[2])) {
				// invalid coefficient name
				tx_string("ERROR\r\n", interface);
				return;
			}
			char filename[50];
			snprintf(filename, sizeof(filename), "%s.%s", argv[2], coefficientOptionEnding(argv[2]));
			if(!Touchstone::DeleteFile(argv[1], filename)) {
				// failed to delete file
				tx_string("ERROR\r\n", interface);
				return;
			}
			// file deleted
			tx_string("\r\n", interface);
		}, nullptr, 2),
		Command("COEFFicient:ADD_COMMENT", [](char *argv[], int argc, int interface){
			char comment[CommentMaxSize+1];
			memset(comment, 0, sizeof(comment));
			// Concatenate all argv(words) adding a space between each word to create the full comment
			for(uint8_t i=0;i<argc - 1;i++) {
				strncat(comment, argv[i+1], (CommentMaxSize-strlen(comment)));
				if(strlen(comment) == CommentMaxSize-1)
					break;
				strncat(comment, " ", (CommentMaxSize-strlen(comment)));
			}
			// Remove last space at end if it exist
			int len = strlen(comment);
			if(comment[len-1] == ' ')
				comment[len-1] = 0;	
			if(comment[len] == ' ')
				comment[len] = 0;
			if(!Touchstone::AddComment(comment)) {
				// failed to add comment
				tx_string("ERROR\r\n", interface);
				return;
			}
			// comment added
			tx_string("\r\n", interface);
		}, nullptr, 1),
		Command("COEFFicient:ADD", [](char *argv[], int argc, int interface){
			double freq = strtod(argv[1], NULL);
			double values[argc - 2];
			for(uint8_t i=0;i<argc - 2;i++) {
				values[i] = strtod(argv[i+2], NULL);
			}
			if(!Touchstone::AddPoint(freq, values, argc - 2)) {
				// failed to add points
				tx_string("ERROR\r\n", interface);
				return;
			}
			// point added
			tx_string("\r\n", interface);
		}, nullptr, 3),
		Command("COEFFicient:FINish", [](char *argv[], int argc, int interface){
			if(!Touchstone::FinishFile()) {
				// failed to finish file
				tx_string("ERROR\r\n", interface);
				return;
			}
			// file finished
			tx_string("\r\n", interface);
		}),
		Command("COEFFicient:NUMber", nullptr, [](char *argv[], int argc, int interface){
			if(!coefficientOptionEnding(argv[2])) {
				// invalid coefficient name
				tx_string("ERROR\r\n", interface);
				return;
			}
			char filename[50];
			snprintf(filename, sizeof(filename), "%s.%s", argv[2], coefficientOptionEnding(argv[2]));
			auto points = Touchstone::GetPointNum(argv[1], filename);
			tx_int(points, interface);
			tx_string("\r\n", interface);
		}, 0, 2),
		Command("COEFFicient:GET", nullptr, [](char *argv[], int argc, int interface){
			if(!coefficientOptionEnding(argv[2])) {
				// invalid coefficient name
				tx_string("ERROR\r\n", interface);
				return;
			}
			char filename[50];
			snprintf(filename, sizeof(filename), "%s.%s", argv[2], coefficientOptionEnding(argv[2]));
			if(argc == 4) {
				// specific point requested
				uint32_t point = strtoul(argv[3], NULL, 10);
				double values[9];
				int decoded = Touchstone::GetPoint(argv[1], filename, point, values);
				if(decoded == 0) {
					tx_string("ERROR\r\n", interface);
					return;
				} else {
					char response[200] = "";
					for(int i=0;i<decoded;i++) {
						char val[20];
						auto len = strlen(response);
						len += snprintf(&response[len], sizeof(response)-len, "%f",values[i]);
						if(i<decoded - 1) {
							// not the last entry
							response[len] = ',';
							response[len+1] = '\0';
						}
					}
					strcat(response, "\r\n");
					tx_string(response, interface);
				}
			} else if(argc == 3) {
				// whole file requested
				if(!Touchstone::PrintFile(argv[1], filename, tx_data, interface)) {
					tx_string("ERROR\r\n", interface);
				}
			} else {
				tx_string("ERROR\r\n", interface);
			}
		}, 0, 2),
		Command("FACTory:ENABLEWRITE", [](char *argv[], int argc, int interface){
			if(strcmp("I_AM_SURE", argv[1]) != 0) {
				tx_string("ERROR\r\n", interface);
				return;
			}
			Touchstone::EnableFactoryWriting();
			tx_string("\r\n", interface);
		}, nullptr, 1),
		Command("FACTory:DELete", [](char *argv[], int argc, int interface){
			if(Touchstone::clearFactory()) {
				tx_string("\r\n", interface);
			} else {
				tx_string("ERROR\r\n", interface);
			}
		}),
		Command("BOOTloader", [](char *argv[], int argc, int interface){
			tx_string("\r\n", interface);
			vTaskDelay(100);
			reset_usb_boot(0, 0);
		}),
		Command("DATE_TIME", [](char *argv[], int argc, int interface){
			int nb_match;
			datetime_t t = { 0 };
			
			if(argc == 4) {
				char* date = argv[1];
				char* time = argv[2];
				char* utc = argv[3];

				if(validate_date(date, &t) == false) {
					tx_string("ERROR date format invalid\r\n", interface);
				} else if(validate_time(time, &t) == false) {
					tx_string("ERROR time format invalid\r\n", interface);
				} else if(validate_utc(utc) == false) {
					tx_string("ERROR UTC format invalid\r\n", interface);
				} else if(rtc_set_datetime(&t) == true) {
					strcpy(scpi_date_time_utc, utc);
					tx_string("\r\n", interface);
				} else {
					tx_string("ERROR rtc_set_datetime()\r\n", interface);
				}
			} else {
				tx_string("ERROR 3 arguments 'DATE' 'TIME' 'UTC' are required\r\n", interface);
			}
		},
		[](char *argv[], int argc, int interface){
			char date_time_utc[32]; // Max 32chars including \r\n\0 char example "2023/03/01 10:05:48 UTC+01:00"
			datetime_t t = { 0 };			
			if(rtc_get_datetime(&t) == true) {
				snprintf(date_time_utc, sizeof(date_time_utc), "%04d/%02d/%02d %02d:%02d:%02d %s\r\n", 
						t.year, t.month, t.day, t.hour, t.min, t.sec, scpi_date_time_utc);
				tx_string(date_time_utc, interface);
			} else {
				tx_string("ERROR\r\n", interface);
			}
		}, 3),
};

static void scpi_lst(char *argv[], int argc, int interface) {
	for(int i=0;i<ARRAY_SIZE(commands);i++) {
		auto c = commands[i];
		if(c.cmd != nullptr) {
			tx_string(c.name, interface);
			tx_string("\r\n", interface);
		}
		if(c.query != nullptr) {
			tx_string(c.name, interface);
			tx_string("?", interface);
			tx_string("\r\n", interface);
		}
	}
}

static void parse(char *s, uint8_t interface) {
	static char last_cmd[ParseLastCmdMaxSize] = ":";
	// split strings into args
	char *argv[ParseArgumentsMax];
	int argc = 0;
	argv[argc] = s;
	while(*s) {
		if(*s == ' ') {
			*s = '\0';
			// found the end of an argument, check if last argument had at least one character (merge multiple spaces)
			if(s - argv[argc] > 0) {
				argc++;
			}
			argv[argc] = s + 1;
		}
		s++;
	}
	if(s - argv[argc] > 0) {
		argc++;
	}
	// not required by the spec - only when using multiple commands per line (which this implementation does not support)
//	if(argv[0][0] != ':' && argv[0][0] != '*') {
//		// remove leave from last_cmd and append argv[0] instead
//		auto leafStart = strrchr(last_cmd, ':');
//		leafStart++;
//		*leafStart = '\0';
//		strncat(last_cmd, argv[0], sizeof(last_cmd) - strlen(last_cmd));
//		// replace argv[0] with newly assembled string
//		argv[0] = last_cmd;
//	}
	for(auto i=0;i<ARRAY_SIZE(commands);i++) {
		if(commands[i].parse(argv, argc, interface)) {
			strncpy(last_cmd, argv[0], sizeof(last_cmd));
			return;
		}
	}
	tx_string("ERROR\r\n", interface);
}

void SCPI::Init(scpi_tx_callback callback) {
	tx_data = callback;
	for(auto i=0;i<NumInterfaces;i++) {
		rx_cnt[i] = 0;
	}
}

void SCPI::Input(const char *msg, uint16_t len, uint8_t interface) {
	if(interface >= NumInterfaces) {
		return;
	}
	auto buf = buffer[interface];
	auto cnt = &rx_cnt[interface];
	if(*cnt + len > BufferSize) {
		// doesn't fit anymore
		return;
	}
	memcpy(&buf[*cnt], msg, len);
	*cnt += len;
	// check if got a complete line
	char *endptr = (char*) memchr(buf, '\n', *cnt);
	while(endptr) {
		uint8_t bytes_line = endptr - buf + 1;
		if(bytes_line > 1) {
			// check if previous char was '\r'
			auto prev = endptr - 1;
			if(*prev == '\r') {
				*prev = 0;
			}
		}
		*endptr = 0;

		parse(buf, interface);

		if(*cnt > bytes_line) {
			// already got bytes afterwards, move them
			memmove(buf, &buf[bytes_line], *cnt - bytes_line);
			*cnt -= bytes_line;
			// check if we have another complete line
			endptr = (char*) memchr(buf, '\n', *cnt);
		} else {
			*cnt = 0;
			// no more data
			endptr = 0;
		}
	}
}

