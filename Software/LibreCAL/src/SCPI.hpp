#pragma once

#include <cstdint>

namespace SCPI {

using scpi_tx_callback = bool(*)(const uint8_t *msg, uint16_t len, uint8_t interface);

void Init(scpi_tx_callback callback);

void Input(const char *msg, uint16_t len, uint8_t interface);

}
