#pragma once

#include <stddef.h>
#include <stdint.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class StorageAdapter {
 public:
  virtual ~StorageAdapter() {}
  virtual bool begin() = 0;
  virtual bool fileExists(const char *path) = 0;
  virtual bool readAt(const char *path, uint32_t offset, void *buf, size_t len) = 0;
  virtual bool writeAt(const char *path, uint32_t offset, const void *buf, size_t len) = 0;
  virtual bool append(const char *path, const void *buf, size_t len) = 0;
  virtual bool createFile(const char *path) = 0;
  virtual bool rename(const char *oldPath, const char *newPath) = 0;
  virtual void end() = 0;
};

class SPIFFSStorageAdapter : public StorageAdapter {
 public:
  explicit SPIFFSStorageAdapter(const std::string &root = "spiffs");
  bool begin() override;
  bool fileExists(const char *path) override;
  bool readAt(const char *path, uint32_t offset, void *buf, size_t len) override;
  bool writeAt(const char *path, uint32_t offset, const void *buf, size_t len) override;
  bool append(const char *path, const void *buf, size_t len) override;
  bool createFile(const char *path) override;
  bool rename(const char *oldPath, const char *newPath) override;
  void end() override;

 private:
  std::string root_;
  bool initialized_ = false;
};

class SDStorageAdapter : public StorageAdapter {
 public:
  explicit SDStorageAdapter(const std::string &root = "sdcard");
  bool begin() override;
  bool fileExists(const char *path) override;
  bool readAt(const char *path, uint32_t offset, void *buf, size_t len) override;
  bool writeAt(const char *path, uint32_t offset, const void *buf, size_t len) override;
  bool append(const char *path, const void *buf, size_t len) override;
  bool createFile(const char *path) override;
  bool rename(const char *oldPath, const char *newPath) override;
  void end() override;

 private:
  std::string root_;
  bool initialized_ = false;
};

class FakeStorageAdapter : public StorageAdapter {
 public:
  bool begin() override;
  bool fileExists(const char *path) override;
  bool readAt(const char *path, uint32_t offset, void *buf, size_t len) override;
  bool writeAt(const char *path, uint32_t offset, const void *buf, size_t len) override;
  bool append(const char *path, const void *buf, size_t len) override;
  bool createFile(const char *path) override;
  bool rename(const char *oldPath, const char *newPath) override;
  void end() override;

 private:
  std::mutex mu_;
  std::unordered_map<std::string, std::vector<uint8_t>> files_;
  bool initialized_ = false;
};