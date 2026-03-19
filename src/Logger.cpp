#include "Logger.h"

#include "Utils.h"

#include <cstdarg>
#include <cstdio>
#include <string>

#if defined(ARDUINO)
#if defined(USE_ARDUINO) || defined(ARDUINO)
#include <Arduino.h>
#endif
#endif

namespace FaceRecognize {

namespace {
constexpr const char *kLogPath = "/logs/faceengine.log";
constexpr const char *kLogRotatePath = "/logs/faceengine.log.1";
constexpr uint32_t kMaxLogBytes = 5u * 1024u * 1024u;
constexpr const char *kModule = "CORE";

void emitSerialLine(const std::string &line) {
#if defined(ARDUINO)
  Serial.println(line.c_str());
#else
  std::fputs(line.c_str(), stdout);
  std::fputc('\n', stdout);
  std::fflush(stdout);
#endif
}

void emitSdErrorLine() {
  const std::string ts = fr::formatTimestampNow();
  emitSerialLine(ts + " | " + kModule + " | SD logging error");
}

}  // namespace

void Logger::begin(bool enableSd) {
  std::lock_guard<std::mutex> lock(mu_);
  enableSd_ = enableSd;
  sdWriteErrorLogged_ = false;
}

void Logger::setRecognitionEngineStartTime(uint32_t t_ms) {
  std::lock_guard<std::mutex> lock(mu_);
  recognitionEngineStartTimeMs_ = t_ms;
}

void Logger::setStorageAdapter(StorageAdapter *adapter) {
  std::lock_guard<std::mutex> lock(mu_);
  storage_ = adapter;
}

bool Logger::shouldWriteSdLocked() const {
  if (!enableSd_ || storage_ == nullptr) {
    return false;
  }
  const uint32_t nowMs = fr::millis();
  const uint32_t elapsed = nowMs - recognitionEngineStartTimeMs_;
  return elapsed >= 2000u;
}

uint32_t Logger::fileSizeLocked(const char *path) const {
  if (storage_ == nullptr || path == nullptr || !storage_->fileExists(path)) {
    return 0;
  }

  uint8_t probe = 0;
  uint64_t low = 0;
  uint64_t high = 1;
  while (high < (1ull << 31) && storage_->readAt(path, static_cast<uint32_t>(high - 1), &probe, 1)) {
    low = high;
    high <<= 1;
  }

  while (low + 1 < high) {
    const uint64_t mid = low + ((high - low) / 2);
    if (storage_->readAt(path, static_cast<uint32_t>(mid - 1), &probe, 1)) {
      low = mid;
    } else {
      high = mid;
    }
  }

  return static_cast<uint32_t>(low);
}

void Logger::writeSdLocked(const char *line, size_t len) {
  if (storage_ == nullptr) {
    return;
  }

  if (!storage_->fileExists(kLogPath) && !storage_->createFile(kLogPath)) {
    if (!sdWriteErrorLogged_) {
      sdWriteErrorLogged_ = true;
      emitSdErrorLine();
    }
    return;
  }

  const uint32_t fileSize = fileSizeLocked(kLogPath);
  if (fileSize >= kMaxLogBytes) {
    storage_->rename(kLogPath, kLogRotatePath);
    storage_->createFile(kLogPath);
  }

  if (!storage_->append(kLogPath, line, len)) {
    if (!sdWriteErrorLogged_) {
      sdWriteErrorLogged_ = true;
      emitSdErrorLine();
    }
    return;
  }

  static const char newline = '\n';
  storage_->append(kLogPath, &newline, 1);
}

void Logger::log(const char *fmt, ...) {
  char message[512];
  va_list args;
  va_start(args, fmt);
  const int n = vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  if (n < 0) {
    return;
  }

  const std::string ts = fr::formatTimestampNow();
  std::string line;
  line.reserve(ts.size() + 8 + static_cast<size_t>(n));
  line.append(ts);
  line.append(" | ");
  line.append(kModule);
  line.append(" | ");
  line.append(message);

  emitSerialLine(line);

  std::lock_guard<std::mutex> lock(mu_);
  if (shouldWriteSdLocked()) {
    writeSdLocked(line.c_str(), line.size());
  }
}

void Logger::shutdown() {
  std::lock_guard<std::mutex> lock(mu_);
  enableSd_ = false;
  storage_ = nullptr;
}

} // namespace FaceRecognize