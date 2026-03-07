#include "StorageAdapter.h"

#include <algorithm>
#include <cstring>
#include <fstream>

#if defined(ARDUINO)
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>
#else
#include <filesystem>
#endif

namespace {

#if !defined(ARDUINO)
std::string normalizePath(const std::string &path) {
  if (path.empty()) {
    return path;
  }
  if (path[0] == '/' || path[0] == '\\') {
    return path.substr(1);
  }
  return path;
}

std::string joinPath(const std::string &root, const std::string &path) {
  const std::string p = normalizePath(path);
  if (root.empty()) {
    return p;
  }
  if (p.empty()) {
    return root;
  }
  return root + "/" + p;
}

bool ensureParent(const std::string &path) {
  std::filesystem::path p(path);
  const auto parent = p.parent_path();
  if (parent.empty()) {
    return true;
  }
  std::error_code ec;
  std::filesystem::create_directories(parent, ec);
  return !ec;
}

bool readFileAt(const std::string &path, uint32_t offset, void *buf, size_t len) {
  std::ifstream in(path, std::ios::binary);
  if (!in.good()) {
    return false;
  }
  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  if (size < 0 || static_cast<uint64_t>(size) < static_cast<uint64_t>(offset) + len) {
    return false;
  }
  in.seekg(offset, std::ios::beg);
  in.read(reinterpret_cast<char *>(buf), static_cast<std::streamsize>(len));
  return in.good();
}

bool writeFileAt(const std::string &path, uint32_t offset, const void *buf, size_t len) {
  if (!ensureParent(path)) {
    return false;
  }

  std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
  if (!io.good()) {
    std::ofstream create(path, std::ios::binary | std::ios::out);
    create.close();
    io.open(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!io.good()) {
      return false;
    }
  }

  io.seekp(0, std::ios::end);
  const std::streamoff size = io.tellp();
  if (size < 0) {
    return false;
  }

  if (static_cast<uint64_t>(size) < offset) {
    io.seekp(0, std::ios::end);
    std::vector<uint8_t> zeros(static_cast<size_t>(offset - size), 0);
    io.write(reinterpret_cast<const char *>(zeros.data()), static_cast<std::streamsize>(zeros.size()));
  }

  io.seekp(offset, std::ios::beg);
  io.write(reinterpret_cast<const char *>(buf), static_cast<std::streamsize>(len));
  io.flush();
  return io.good();
}

bool appendFile(const std::string &path, const void *buf, size_t len) {
  if (!ensureParent(path)) {
    return false;
  }
  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out.good()) {
    return false;
  }
  out.write(reinterpret_cast<const char *>(buf), static_cast<std::streamsize>(len));
  out.flush();
  return out.good();
}

bool createFileOnDisk(const std::string &path) {
  if (!ensureParent(path)) {
    return false;
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  return out.good();
}

bool renameFile(const std::string &oldPath, const std::string &newPath) {
  if (!ensureParent(newPath)) {
    return false;
  }
  std::error_code ec;
  std::filesystem::remove(newPath, ec);
  ec.clear();
  std::filesystem::rename(oldPath, newPath, ec);
  return !ec;
}

bool existsFile(const std::string &path) {
  std::error_code ec;
  return std::filesystem::exists(path, ec);
}
#endif

}  // namespace

SPIFFSStorageAdapter::SPIFFSStorageAdapter(const std::string &root) : root_(root) {}

bool SPIFFSStorageAdapter::begin() {
#if defined(ARDUINO)
  initialized_ = LittleFS.begin(true);
#else
  std::error_code ec;
  std::filesystem::create_directories(root_, ec);
  initialized_ = !ec;
#endif
  return initialized_;
}

bool SPIFFSStorageAdapter::fileExists(const char *path) {
  if (!initialized_ || path == nullptr) {
    return false;
  }
#if defined(ARDUINO)
  return LittleFS.exists(path);
#else
  return existsFile(joinPath(root_, path));
#endif
}

bool SPIFFSStorageAdapter::readAt(const char *path, uint32_t offset, void *buf, size_t len) {
  if (!initialized_ || path == nullptr || buf == nullptr) {
    return false;
  }
#if defined(ARDUINO)
  File f = LittleFS.open(path, "r");
  if (!f) {
    return false;
  }
  if (!f.seek(offset)) {
    f.close();
    return false;
  }
  const size_t n = f.read(reinterpret_cast<uint8_t *>(buf), len);
  f.close();
  return n == len;
#else
  return readFileAt(joinPath(root_, path), offset, buf, len);
#endif
}

bool SPIFFSStorageAdapter::writeAt(const char *path, uint32_t offset, const void *buf, size_t len) {
  if (!initialized_ || path == nullptr || buf == nullptr) {
    return false;
  }
#if defined(ARDUINO)
  // Arduino FS does not guarantee sparse writes; rewrite through append when needed.
  File f = LittleFS.open(path, "r+");
  if (!f) {
    f = LittleFS.open(path, "w+");
    if (!f) {
      return false;
    }
  }
  if (!f.seek(offset)) {
    f.close();
    return false;
  }
  const size_t n = f.write(reinterpret_cast<const uint8_t *>(buf), len);
  f.flush();
  f.close();
  return n == len;
#else
  return writeFileAt(joinPath(root_, path), offset, buf, len);
#endif
}

bool SPIFFSStorageAdapter::append(const char *path, const void *buf, size_t len) {
  if (!initialized_ || path == nullptr || buf == nullptr) {
    return false;
  }
#if defined(ARDUINO)
  File f = LittleFS.open(path, "a");
  if (!f) {
    return false;
  }
  const size_t n = f.write(reinterpret_cast<const uint8_t *>(buf), len);
  f.flush();
  f.close();
  return n == len;
#else
  return appendFile(joinPath(root_, path), buf, len);
#endif
}

bool SPIFFSStorageAdapter::createFile(const char *path) {
  if (!initialized_ || path == nullptr) {
    return false;
  }
#if defined(ARDUINO)
  File f = LittleFS.open(path, "w");
  const bool ok = static_cast<bool>(f);
  if (f) {
    f.close();
  }
  return ok;
#else
  return createFileOnDisk(joinPath(root_, path));
#endif
}

bool SPIFFSStorageAdapter::rename(const char *oldPath, const char *newPath) {
  if (!initialized_ || oldPath == nullptr || newPath == nullptr) {
    return false;
  }
#if defined(ARDUINO)
  LittleFS.remove(newPath);
  return LittleFS.rename(oldPath, newPath);
#else
  return renameFile(joinPath(root_, oldPath), joinPath(root_, newPath));
#endif
}

void SPIFFSStorageAdapter::end() {
  initialized_ = false;
}

SDStorageAdapter::SDStorageAdapter(const std::string &root) : root_(root) {}

bool SDStorageAdapter::begin() {
#if defined(ARDUINO)
  initialized_ = SD.begin();
#else
  std::error_code ec;
  std::filesystem::create_directories(root_, ec);
  initialized_ = !ec;
#endif
  return initialized_;
}

bool SDStorageAdapter::fileExists(const char *path) {
  if (!initialized_ || path == nullptr) {
    return false;
  }
#if defined(ARDUINO)
  return SD.exists(path);
#else
  return existsFile(joinPath(root_, path));
#endif
}

bool SDStorageAdapter::readAt(const char *path, uint32_t offset, void *buf, size_t len) {
  if (!initialized_ || path == nullptr || buf == nullptr) {
    return false;
  }
#if defined(ARDUINO)
  File f = SD.open(path, FILE_READ);
  if (!f) {
    return false;
  }
  if (!f.seek(offset)) {
    f.close();
    return false;
  }
  const size_t n = f.read(reinterpret_cast<uint8_t *>(buf), len);
  f.close();
  return n == len;
#else
  return readFileAt(joinPath(root_, path), offset, buf, len);
#endif
}

bool SDStorageAdapter::writeAt(const char *path, uint32_t offset, const void *buf, size_t len) {
  if (!initialized_ || path == nullptr || buf == nullptr) {
    return false;
  }
#if defined(ARDUINO)
  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    return false;
  }
  if (!f.seek(offset)) {
    f.close();
    return false;
  }
  const size_t n = f.write(reinterpret_cast<const uint8_t *>(buf), len);
  f.flush();
  f.close();
  return n == len;
#else
  return writeFileAt(joinPath(root_, path), offset, buf, len);
#endif
}

bool SDStorageAdapter::append(const char *path, const void *buf, size_t len) {
  if (!initialized_ || path == nullptr || buf == nullptr) {
    return false;
  }
#if defined(ARDUINO)
  File f = SD.open(path, FILE_APPEND);
  if (!f) {
    return false;
  }
  const size_t n = f.write(reinterpret_cast<const uint8_t *>(buf), len);
  f.flush();
  f.close();
  return n == len;
#else
  return appendFile(joinPath(root_, path), buf, len);
#endif
}

bool SDStorageAdapter::createFile(const char *path) {
  if (!initialized_ || path == nullptr) {
    return false;
  }
#if defined(ARDUINO)
  File f = SD.open(path, FILE_WRITE);
  const bool ok = static_cast<bool>(f);
  if (f) {
    f.close();
  }
  return ok;
#else
  return createFileOnDisk(joinPath(root_, path));
#endif
}

bool SDStorageAdapter::rename(const char *oldPath, const char *newPath) {
  if (!initialized_ || oldPath == nullptr || newPath == nullptr) {
    return false;
  }
#if defined(ARDUINO)
  SD.remove(newPath);
  return SD.rename(oldPath, newPath);
#else
  return renameFile(joinPath(root_, oldPath), joinPath(root_, newPath));
#endif
}

void SDStorageAdapter::end() {
  initialized_ = false;
}

bool FakeStorageAdapter::begin() {
  std::lock_guard<std::mutex> lock(mu_);
  initialized_ = true;
  return true;
}

bool FakeStorageAdapter::fileExists(const char *path) {
  if (path == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (!initialized_) {
    return false;
  }
  return files_.find(path) != files_.end();
}

bool FakeStorageAdapter::readAt(const char *path, uint32_t offset, void *buf, size_t len) {
  if (path == nullptr || buf == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (!initialized_) {
    return false;
  }
  auto it = files_.find(path);
  if (it == files_.end()) {
    return false;
  }
  const auto &data = it->second;
  if (static_cast<uint64_t>(offset) + len > data.size()) {
    return false;
  }
  std::memcpy(buf, data.data() + offset, len);
  return true;
}

bool FakeStorageAdapter::writeAt(const char *path, uint32_t offset, const void *buf, size_t len) {
  if (path == nullptr || buf == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (!initialized_) {
    return false;
  }
  auto &data = files_[path];
  if (data.size() < static_cast<size_t>(offset) + len) {
    data.resize(static_cast<size_t>(offset) + len, 0);
  }
  std::memcpy(data.data() + offset, buf, len);
  return true;
}

bool FakeStorageAdapter::append(const char *path, const void *buf, size_t len) {
  if (path == nullptr || buf == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (!initialized_) {
    return false;
  }
  auto &data = files_[path];
  const size_t start = data.size();
  data.resize(start + len);
  std::memcpy(data.data() + start, buf, len);
  return true;
}

bool FakeStorageAdapter::createFile(const char *path) {
  if (path == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (!initialized_) {
    return false;
  }
  files_[path] = {};
  return true;
}

bool FakeStorageAdapter::rename(const char *oldPath, const char *newPath) {
  if (oldPath == nullptr || newPath == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (!initialized_) {
    return false;
  }
  auto it = files_.find(oldPath);
  if (it == files_.end()) {
    return false;
  }
  files_[newPath] = std::move(it->second);
  files_.erase(it);
  return true;
}

void FakeStorageAdapter::end() {
  std::lock_guard<std::mutex> lock(mu_);
  initialized_ = false;
}