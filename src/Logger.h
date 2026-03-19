#pragma once

#include "../adapters/StorageAdapter.h"

#include <mutex>

namespace FaceRecognize {

class Logger {
 public:
  void begin(bool enableSd = false);
  void setRecognitionEngineStartTime(uint32_t t_ms);
  void setStorageAdapter(StorageAdapter *adapter);
  void log(const char *fmt, ...);
  void shutdown();

  static Logger& getInstance() {
    static Logger instance;
    return instance;
  }

 private:
  bool shouldWriteSdLocked() const;
  void writeSdLocked(const char *line, size_t len);
  uint32_t fileSizeLocked(const char *path) const;

  mutable std::mutex mu_;
  bool enableSd_ = false;
  bool sdWriteErrorLogged_ = false;
  uint32_t recognitionEngineStartTimeMs_ = 0;
  StorageAdapter *storage_ = nullptr;
};

} // namespace FaceRecognize