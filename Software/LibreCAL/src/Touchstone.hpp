/*
 * touchstone.hpp
 *
 *  Created on: Sep 5, 2022
 *      Author: jan
 */

#ifndef SRC_TOUCHSTONE_HPP_
#define SRC_TOUCHSTONE_HPP_

#include <cstdint>

#include "SCPI.hpp"

namespace Touchstone {

uint32_t GetPointNum(const char *folder, const char *filename);
bool StartNewFile(const char *folder, const char *filename);
bool AddComment(const char* comment);
bool AddPoint(double frequency, double *values, uint8_t num_values);
bool FinishFile();
bool DeleteFile(const char *folder, const char *filename);
int GetPoint(const char *folder, const char *filename, uint32_t point, double *values);
bool GetUserCoefficientName(uint8_t index, char *name, uint16_t maxlen);
bool PrintFile(const char *folder, const char *filename, SCPI::scpi_tx_callback tx_func, uint8_t interface);

void EnableFactoryWriting();
bool clearFactory();

};

#endif /* SRC_TOUCHSTONE_HPP_ */
