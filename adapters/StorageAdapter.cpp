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
  if (path.empty()) return path;
  if (path[0] == '/' || path[0] == '\\') return path.substr(1);
  return path;
}

std::string joinPath(const std::string &root, const std::string &path) {
  const std::string p = normalizePath(path);
  if (root.empty()) return p;
  if (p.empty()) return root;
  return root + "/" + p;
}

bool ensureParent(const std::string &path) {
  std::filesystem::path p(path);
  const auto parent = p.parent_path();
  if (parent.empty()) return true;
  std::error_code ec;
  std::filesystem::create_directories(parent, ec);
  return !ec;
}

bool readFileAt(const std::string &path, uint32_t offset, void *buf, size_t len) {
  std::ifstream in(path, std::ios::binary);
  if (!in.good()) return false;
  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  if (size < 0 || static_cast<uint64_t>(size) < static_cast<uint64_t>(offset) + len) return false;
  in.seekg(offset, std::ios::beg);
  in.read(reinterpret_cast<char *>(buf), static_cast<std::streamsize>(len));
  return in.good();
}

bool writeFileAt(const std::string &path, uint32_t offset, const void *buf, size_t len) {
  if (!ensureParent(path)) return false;
  std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
  if (!io.good()) {
    std::ofstream create(path, std::ios::binary | std::ios::out);
    create.close();
    io.open(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!io.good()) return false;
  }
  io.seekp(offset, std::ios::beg);
  io.write(reinterpret_cast<const char *>(buf), static_cast<std::streamsize>(len));
  return io.good();
}

bool appendFile(const std::string &path, const void *buf, size_t len) {
  if (!ensureParent(path)) return false;
  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out.good()) return false;
  out.write(reinterpret_cast<const char *>(buf), static_cast<std::streamsize>(len));
  return out.good();
}

bool createFileOnDisk(const std::string &path) {
  if (!ensureParent(path)) return false;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  return out.good();
}

bool renameFile(const std::string &oldPath, const std::string &newPath) {
  std::error_code ec;
  std::filesystem::rename(oldPath, newPath, ec);
  return !ec;
}

bool existsFile(const std::string &path) {
  return std::filesystem::exists(path);
}
#endif

}  // namespace

namespace FaceRecognize {

// SPIFFS Implementation
SPIFFSStorageAdapter::SPIFFSStorageAdapter(const std::string &root) : root_(root) {}
bool SPIFFSStorageAdapter::begin() { 
#if defined(ARDUINO)
    initialized_ = LittleFS.begin(true); 
#else
    initialized_ = true;
#endif
    return initialized_; 
}
bool SPIFFSStorageAdapter::fileExists(const char *path) { 
#if defined(ARDUINO)
    return LittleFS.exists(path);
#else
    return existsFile(joinPath(root_, path)); 
#endif
}
bool SPIFFSStorageAdapter::readAt(const char *path, uint32_t offset, void *buf, size_t len) {
#if defined(ARDUINO)
    fs::File f = LittleFS.open(path, "r");
    if (!f) return false;
    if (!f.seek(offset)) return false;
    return f.read((uint8_t*)buf, len) == len;
#else
    return readFileAt(joinPath(root_, path), offset, buf, len);
#endif
}
bool SPIFFSStorageAdapter::writeAt(const char *path, uint32_t offset, const void *buf, size_t len) {
#if defined(ARDUINO)
    fs::File f = LittleFS.open(path, "r+");
    if (!f) f = LittleFS.open(path, "w");
    if (!f) return false;
    if (!f.seek(offset)) return false;
    return f.write((const uint8_t*)buf, len) == len;
#else
    return writeFileAt(joinPath(root_, path), offset, buf, len);
#endif
}
bool SPIFFSStorageAdapter::append(const char *path, const void *buf, size_t len) {
#if defined(ARDUINO)
    fs::File f = LittleFS.open(path, "a");
    if (!f) return false;
    return f.write((const uint8_t*)buf, len) == len;
#else
    return appendFile(joinPath(root_, path), buf, len);
#endif
}
bool SPIFFSStorageAdapter::createFile(const char *path) {
#if defined(ARDUINO)
    fs::File f = LittleFS.open(path, "w");
    return (bool)f;
#else
    return createFileOnDisk(joinPath(root_, path));
#endif
}
bool SPIFFSStorageAdapter::rename(const char *oldPath, const char *newPath) {
#if defined(ARDUINO)
    return LittleFS.rename(oldPath, newPath);
#else
    return renameFile(joinPath(root_, oldPath), joinPath(root_, newPath));
#endif
}
void SPIFFSStorageAdapter::end() { initialized_ = false; }

// SD Implementation
SDStorageAdapter::SDStorageAdapter(const std::string &root) : root_(root) {}
bool SDStorageAdapter::begin() { initialized_ = true; return true; }
bool SDStorageAdapter::fileExists(const char *path) { 
#if defined(ARDUINO)
    return SD.exists(path);
#else
    return existsFile(joinPath(root_, path)); 
#endif
}
bool SDStorageAdapter::readAt(const char *path, uint32_t offset, void *buf, size_t len) {
#if defined(ARDUINO)
    fs::File f = SD.open(path, "r");
    if (!f) return false;
    if (!f.seek(offset)) return false;
    return f.read((uint8_t*)buf, len) == len;
#else
    return readFileAt(joinPath(root_, path), offset, buf, len);
#endif
}
bool SDStorageAdapter::writeAt(const char *path, uint32_t offset, const void *buf, size_t len) {
#if defined(ARDUINO)
    fs::File f = SD.open(path, "r+");
    if (!f) f = SD.open(path, "w");
    if (!f) return false;
    if (!f.seek(offset)) return false;
    return f.write((const uint8_t*)buf, len) == len;
#else
    return writeFileAt(joinPath(root_, path), offset, buf, len);
#endif
}
bool SDStorageAdapter::append(const char *path, const void *buf, size_t len) {
#if defined(ARDUINO)
    fs::File f = SD.open(path, "a");
    if (!f) return false;
    return f.write((const uint8_t*)buf, len) == len;
#else
    return appendFile(joinPath(root_, path), buf, len);
#endif
}
bool SDStorageAdapter::createFile(const char *path) {
#if defined(ARDUINO)
    fs::File f = SD.open(path, "w");
    return (bool)f;
#else
    return createFileOnDisk(joinPath(root_, path));
#endif
}
bool SDStorageAdapter::rename(const char *oldPath, const char *newPath) {
#if defined(ARDUINO)
    return SD.rename(oldPath, newPath);
#else
    return renameFile(joinPath(root_, oldPath), joinPath(root_, newPath));
#endif
}
void SDStorageAdapter::end() { initialized_ = false; }

// Fake Implementation
bool FakeStorageAdapter::begin() { initialized_ = true; return true; }
bool FakeStorageAdapter::fileExists(const char *path) {
    std::lock_guard<std::mutex> lock(mu_);
    return files_.find(path) != files_.end();
}
bool FakeStorageAdapter::readAt(const char *path, uint32_t offset, void *buf, size_t len) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = files_.find(path);
    if (it == files_.end()) return false;
    if (offset + len > it->second.size()) return false;
    std::memcpy(buf, it->second.data() + offset, len);
    return true;
}
bool FakeStorageAdapter::writeAt(const char *path, uint32_t offset, const void *buf, size_t len) {
    std::lock_guard<std::mutex> lock(mu_);
    auto &vec = files_[path];
    if (offset + len > vec.size()) vec.resize(offset + len);
    std::memcpy(vec.data() + offset, buf, len);
    return true;
}
bool FakeStorageAdapter::append(const char *path, const void *buf, size_t len) {
    std::lock_guard<std::mutex> lock(mu_);
    auto &vec = files_[path];
    size_t oldLen = vec.size();
    vec.resize(oldLen + len);
    std::memcpy(vec.data() + oldLen, buf, len);
    return true;
}
bool FakeStorageAdapter::createFile(const char *path) {
    std::lock_guard<std::mutex> lock(mu_);
    files_[path] = std::vector<uint8_t>();
    return true;
}
bool FakeStorageAdapter::rename(const char *oldPath, const char *newPath) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = files_.find(oldPath);
    if (it == files_.end()) return false;
    files_[newPath] = std::move(it->second);
    files_.erase(it);
    return true;
}
void FakeStorageAdapter::end() { initialized_ = false; }

} // namespace FaceRecognize