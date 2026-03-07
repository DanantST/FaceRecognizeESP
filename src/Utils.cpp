#include "Utils.h"

#include <thread>
#include <cstdio>
#include <time.h>

#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace {
const auto kStartClock = std::chrono::steady_clock::now();
}

namespace fr {

uint16_t readLe16(const uint8_t *buf) {
  return static_cast<uint16_t>(buf[0]) |
         (static_cast<uint16_t>(buf[1]) << 8);
}

uint32_t readLe32(const uint8_t *buf) {
  return static_cast<uint32_t>(buf[0]) |
         (static_cast<uint32_t>(buf[1]) << 8) |
         (static_cast<uint32_t>(buf[2]) << 16) |
         (static_cast<uint32_t>(buf[3]) << 24);
}

void writeLe16(uint8_t *buf, uint16_t value) {
  buf[0] = static_cast<uint8_t>(value & 0xFFu);
  buf[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
}

void writeLe32(uint8_t *buf, uint32_t value) {
  buf[0] = static_cast<uint8_t>(value & 0xFFu);
  buf[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
  buf[2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
  buf[3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
}

uint32_t millis() {
#if defined(ARDUINO)
  return ::millis();
#else
  const auto now = std::chrono::steady_clock::now();
  return static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now - kStartClock).count());
#endif
}

void yieldTask() {
#if defined(ARDUINO)
  ::yield();
#else
  std::this_thread::yield();
#endif
}

std::string formatTimestampNow() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  const time_t t = system_clock::to_time_t(now);

  struct tm tmv;
#if defined(_WIN32)
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif

  char buf[40];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
  char out[48];
  snprintf(out, sizeof(out), "%s.%03u", buf, static_cast<unsigned>(ms.count()));
  return out;
}

}  // namespace fr