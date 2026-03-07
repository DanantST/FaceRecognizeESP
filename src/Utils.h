#pragma once

#include <stdint.h>
#include <stddef.h>

#include <chrono>
#include <string>

namespace fr {

uint16_t readLe16(const uint8_t *buf);
uint32_t readLe32(const uint8_t *buf);
void writeLe16(uint8_t *buf, uint16_t value);
void writeLe32(uint8_t *buf, uint32_t value);

uint32_t millis();
void yieldTask();
std::string formatTimestampNow();

}  // namespace fr