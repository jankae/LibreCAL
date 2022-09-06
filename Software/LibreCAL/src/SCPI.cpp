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

#include "FreeRTOS.h"
#include "task.h"

using namespace SCPI;

constexpr int NumInterfaces = 2;
constexpr int BufferSize = 256;

static char buffer[NumInterfaces][BufferSize];
static uint16_t rx_cnt[NumInterfaces];

static scpi_tx_callback tx_data;

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

class Command {
public:
	using Cmd = void(*)(char *argv[], int argc, int interface);
	using Query = void(*)(char *argv[], int argc, int interface);

	Command(const char *name, Cmd cmd = nullptr, Query query = nullptr, int min_args_cmd = 0, int min_args_query = 0) :
		name(name), cmd(cmd), query(query), min_args_cmd(min_args_cmd), min_args_query(min_args_query) {}

	bool parse(char *argv[], int argc, uint8_t interface) const {
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
			tx_string("LibreCAL_", interface);
			tx_string(getSerial(), interface);
			tx_string("\r\n", interface);
		}),
		Command("*LST", nullptr, scpi_lst),
		Command(":FIRMWARE", nullptr, [](char *argv[], int argc, int interface){
			char resp[20];
			snprintf(resp, sizeof(resp), "%d.%d.%d\r\n", FW_MAJOR, FW_MINOR, FW_PATCH);
			tx_string(resp, interface);
		}),
		Command(":PORTS", nullptr, [](char *argv[], int argc, int interface){
			tx_string("4\r\n", interface);
		}),
		Command(":TEMPerature", [](char *argv[], int argc, int interface){
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
		Command(":TEMPerature:STABLE", nullptr,
		[](char *argv[], int argc, int interface){
			if(Heater::IsStable()) {
				tx_string("TRUE\r\n", interface);
			} else {
				tx_string("FALSE\r\n", interface);
			}
		}),
		Command(":HEATer:POWer", nullptr, [](char *argv[], int argc, int interface){
			char resp[20];
			snprintf(resp, sizeof(resp), "%f\r\n", Heater::GetPower());
			tx_string(resp, interface);
		}),
		Command(":PORT", [](char *argv[], int argc, int interface){
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
					Switch::SetStandard(port - 1, s);
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
			tx_string(Switch::StandardName(Switch::GetStandard(port - 1)), interface);
			tx_string("\r\n", interface);
		}, 2, 1),
		Command(":COEFFicient:LIST", nullptr, [](char *argv[], int argc, int interface){
			tx_string("FACTORY", interface);
			uint8_t i=0;
			char name[50];
			while(Touchstone::GetUserCoefficientName(i, name, sizeof(name))) {
				tx_string(",", interface);
				tx_string(name, interface);
			}
			tx_string("\r\n", interface);
		}),
		Command(":COEFFicient:CREATE", [](char *argv[], int argc, int interface){
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
		Command(":COEFFicient:DELete", [](char *argv[], int argc, int interface){
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
		Command(":COEFFicient:ADD", [](char *argv[], int argc, int interface){
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
		Command(":COEFFicient:FINish", [](char *argv[], int argc, int interface){
			if(!Touchstone::FinishFile()) {
				// failed to finish file
				tx_string("ERROR\r\n", interface);
				return;
			}
			// file finished
			tx_string("\r\n", interface);
		}),
		Command(":COEFFicient:NUMber", nullptr, [](char *argv[], int argc, int interface){
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
		Command(":COEFFicient:GET", nullptr, [](char *argv[], int argc, int interface){
			if(!coefficientOptionEnding(argv[2])) {
				// invalid coefficient name
				tx_string("ERROR\r\n", interface);
				return;
			}
			char filename[50];
			snprintf(filename, sizeof(filename), "%s.%s", argv[2], coefficientOptionEnding(argv[2]));
			uint32_t point = strtoul(argv[3], NULL, 10);
			double values[9];
			int decoded = Touchstone::GetPoint(argv[1], filename, point, values);
			if(decoded == 0) {
				tx_string("ERROR\r\n", interface);
				return;
			} else {
				for(int i=0;i<decoded;i++) {
					tx_double(values[i], interface);
					if(i<decoded - 1) {
						// not the last entry
						tx_string(",", interface);
					}
				}
				tx_string("\r\n", interface);
			}
		}, 0, 3),
		Command(":FACTory:ENABLEWRITE", [](char *argv[], int argc, int interface){
			if(strcmp("I_AM_SURE", argv[1]) != 0) {
				tx_string("ERROR\r\n", interface);
				return;
			}
			tx_string("\r\n", interface);
		}, nullptr, 1),
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
	static char last_cmd[50] = ":";
	// split strings into args
	char *argv[20];
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
	if(argv[0][0] != ':' && argv[0][0] != '*') {
		// remove leave from last_cmd and append argv[0] instead
		auto leafStart = strrchr(last_cmd, ':');
		leafStart++;
		*leafStart = '\0';
		strncat(last_cmd, argv[0], sizeof(last_cmd) - strlen(last_cmd));
		// replace argv[0] with newly assembled string
		argv[0] = last_cmd;
	}
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
	char *endptr = (char*) memchr(buf, '\n', len);
	if(endptr) {
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
		} else {
			*cnt = 0;
		}
	}
}
