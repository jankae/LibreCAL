#include "SCPI.hpp"

#include "serial.h"

#include <cstring>
#include <cctype>

using namespace SCPI;

constexpr int NumInterfaces = 2;
constexpr int BufferSize = 256;

static char buffer[NumInterfaces][BufferSize];
static uint16_t rx_cnt[NumInterfaces];

static scpi_tx_callback tx_data;

static void tx_string(const char *s, uint8_t interface) {
	tx_data((const uint8_t*) s, strlen(s), interface);
}

class Command {
public:
	using Cmd = void(*)(char *argv[], int argc, int interface);
	using Query = void(*)(char *argv[], int argc, int interface);

	Command(const char *name, Cmd cmd = nullptr, Query query = nullptr, int min_args = 0) :
		name(name), cmd(cmd), query(query), min_args(min_args) {}

	bool parse(char *argv[], int argc, uint8_t interface) const {
		if(argc - 1 < min_args) {
			return false;
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
						nameOffset = name_len - i - 1;
					}
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
			cmd(argv, argc, interface);
			return true;
		} else if(argv[0][i] == '?' && query != nullptr) {
			query(argv, argc, interface);
			return true;
		}
		// cmd or query not available
		return false;
	}

private:
	const char *name;
	Cmd cmd;
	Query query;
	int min_args;
};

static const Command commands[] = {
		Command("*IDN", nullptr, [](char *argv[], int argc, int interface){
			tx_string("LibreCAL_", interface);
			tx_string(getSerial(), interface);
			tx_string("\r\n", interface);
		}),
};

#define ARRAY_SIZE(d) (sizeof(d)/sizeof(d[0]))

static void parse(char *s, uint8_t interface) {
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
	for(auto i=0;i<ARRAY_SIZE(commands);i++) {
		if(commands[i].parse(argv, argc, interface)) {
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
