#include "FaceDatabase.h"

#include "Utils.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>

namespace {
constexpr uint8_t kMagic[4] = {'F', 'R', 'E', 'C'};
constexpr uint16_t kVersion = 1;
constexpr size_t kHeaderSize = 64;
constexpr size_t kIndexEntrySize = 32;

uint32_t nowUnixSeconds() {
  return static_cast<uint32_t>(time(nullptr));
}

void writeFloatLe(uint8_t *buf, float value) {
  static_assert(sizeof(float) == 4, "float must be 32-bit");
  uint32_t raw = 0;
  std::memcpy(&raw, &value, sizeof(float));
  fr::writeLe32(buf, raw);
}

float readFloatLe(const uint8_t *buf) {
  const uint32_t raw = fr::readLe32(buf);
  float out = 0.0f;
  std::memcpy(&out, &raw, sizeof(float));
  return out;
}

std::vector<float> centroidFromTemplates(const std::vector<std::vector<float>> &templates, int featureDim) {
  std::vector<float> centroid(static_cast<size_t>(featureDim), 0.0f);
  if (templates.empty()) {
    return centroid;
  }

  for (const auto &t : templates) {
    for (int i = 0; i < featureDim && i < static_cast<int>(t.size()); ++i) {
      centroid[static_cast<size_t>(i)] += t[static_cast<size_t>(i)];
    }
  }

  const float invN = 1.0f / static_cast<float>(templates.size());
  for (float &v : centroid) {
    v *= invN;
  }
  return centroid;
}

}  // namespace

FaceDatabase::FaceDatabase(StorageAdapter &storage,
                           int featureDim,
                           int maxUsers,
                           int templatesPerUser,
                           bool loadCentroidsToRam,
                           const std::string &dbPath)
    : storage_(storage),
      featureDim_(featureDim),
      maxUsers_(maxUsers),
      templatesPerUser_(templatesPerUser),
      loadCentroidsToRam_(loadCentroidsToRam),
      dbPath_(dbPath),
      tmpPath_(dbPath + ".tmp"),
      bakPath_(dbPath + ".bak") {}

bool FaceDatabase::begin() {
  std::lock_guard<std::mutex> lock(mu_);
  if (started_) {
    return true;
  }

  if (!storage_.begin()) {
    return false;
  }

  std::vector<UserData> loaded;
  if (storage_.fileExists(dbPath_.c_str())) {
    if (!loadDbLocked(dbPath_, loaded)) {
      if (!storage_.fileExists(bakPath_.c_str()) || !loadDbLocked(bakPath_, loaded)) {
        return false;
      }
      const std::vector<UserData> backupUsers = loaded;
      if (!writeDbLocked(backupUsers)) {
        return false;
      }
    }
  } else {
    loaded.clear();
    if (!writeDbLocked(loaded)) {
      return false;
    }
  }

  if (loadCentroidsToRam_) {
    rebuildCentroidsLocked(loaded);
  }
  users_ = std::move(loaded);
  started_ = true;
  return true;
}

void FaceDatabase::shutdown() {
  std::lock_guard<std::mutex> lock(mu_);
  users_.clear();
  started_ = false;
  storage_.end();
}

bool FaceDatabase::enrollUser(uint32_t userId,
                              const std::vector<std::vector<float>> &templates,
                              const std::vector<float> &centroid,
                              uint32_t &outTimestamp) {
  std::lock_guard<std::mutex> lock(mu_);
  if (!started_ || templates.size() != static_cast<size_t>(templatesPerUser_)) {
    return false;
  }
  if (users_.size() >= static_cast<size_t>(maxUsers_)) {
    return false;
  }

  for (const auto &u : users_) {
    if (u.userId == userId) {
      return false;
    }
  }

  for (const auto &t : templates) {
    if (t.size() != static_cast<size_t>(featureDim_)) {
      return false;
    }
  }

  std::vector<UserData> nextUsers = users_;
  UserData u;
  u.userId = userId;
  u.timestamp = nowUnixSeconds();
  u.templates = templates;
  u.centroid = centroid.size() == static_cast<size_t>(featureDim_)
                   ? centroid
                   : centroidFromTemplates(templates, featureDim_);
  nextUsers.push_back(std::move(u));

  if (!writeDbLocked(nextUsers)) {
    return false;
  }

  users_ = std::move(nextUsers);
  outTimestamp = users_.back().timestamp;
  return true;
}

int FaceDatabase::getNumUsers() const {
  std::lock_guard<std::mutex> lock(mu_);
  return static_cast<int>(users_.size());
}

std::vector<std::pair<uint32_t, std::vector<float>>> FaceDatabase::getCentroids() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<std::pair<uint32_t, std::vector<float>>> out;
  out.reserve(users_.size());
  for (const auto &u : users_) {
    out.push_back({u.userId, u.centroid});
  }
  return out;
}

bool FaceDatabase::loadUserTemplates(uint32_t userId, std::vector<std::vector<float>> &outTemplates) const {
  std::lock_guard<std::mutex> lock(mu_);
  for (const auto &u : users_) {
    if (u.userId == userId) {
      outTemplates = u.templates;
      return true;
    }
  }
  return false;
}

std::string FaceDatabase::diagnostics() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::string s = "db_path=" + dbPath_;
  s += ";users=" + std::to_string(users_.size());
  s += ";feature_dim=" + std::to_string(featureDim_);
  s += ";templates_per_user=" + std::to_string(templatesPerUser_);
  return s;
}

bool FaceDatabase::loadDbLocked(const std::string &path, std::vector<UserData> &outUsers) {
  std::array<uint8_t, kHeaderSize> header{};
  if (!storage_.readAt(path.c_str(), 0, header.data(), header.size())) {
    return false;
  }
  if (std::memcmp(header.data(), kMagic, sizeof(kMagic)) != 0) {
    return false;
  }

  const uint16_t version = fr::readLe16(header.data() + 4);
  const uint16_t featureDim = fr::readLe16(header.data() + 6);
  const uint32_t maxUsers = fr::readLe32(header.data() + 8);
  const uint32_t numUsers = fr::readLe32(header.data() + 12);
  const uint32_t indexOffset = fr::readLe32(header.data() + 16);

  if (version != kVersion || featureDim != static_cast<uint16_t>(featureDim_) ||
      maxUsers > static_cast<uint32_t>(maxUsers_)) {
    return false;
  }

  std::vector<UserData> users;
  users.reserve(numUsers);

  for (uint32_t i = 0; i < numUsers; ++i) {
    std::array<uint8_t, kIndexEntrySize> idx{};
    const uint32_t off = indexOffset + static_cast<uint32_t>(i * kIndexEntrySize);
    if (!storage_.readAt(path.c_str(), off, idx.data(), idx.size())) {
      return false;
    }

    UserData u;
    u.userId = fr::readLe32(idx.data() + 0);
    u.timestamp = fr::readLe32(idx.data() + 4);
    u.firstTemplateOffset = fr::readLe32(idx.data() + 8);
    const uint16_t numTemplates = fr::readLe16(idx.data() + 12);
    if (numTemplates != static_cast<uint16_t>(templatesPerUser_)) {
      return false;
    }

    u.templates.resize(numTemplates, std::vector<float>(static_cast<size_t>(featureDim_)));
    uint32_t templateOffset = u.firstTemplateOffset;
    for (uint16_t t = 0; t < numTemplates; ++t) {
      std::vector<uint8_t> rec(4 + static_cast<size_t>(featureDim_) * 4u, 0);
      if (!storage_.readAt(path.c_str(), templateOffset, rec.data(), rec.size())) {
        return false;
      }

      for (int d = 0; d < featureDim_; ++d) {
        const float v = readFloatLe(rec.data() + 4 + d * 4);
        u.templates[static_cast<size_t>(t)][static_cast<size_t>(d)] = v;
      }
      templateOffset += static_cast<uint32_t>(rec.size());
    }
    users.push_back(std::move(u));
  }

  if (loadCentroidsToRam_) {
    rebuildCentroidsLocked(users);
  }
  outUsers = std::move(users);
  return true;
}

bool FaceDatabase::writeDbLocked(const std::vector<UserData> &users) {
  if (!storage_.createFile(tmpPath_.c_str())) {
    return false;
  }

  std::array<uint8_t, kHeaderSize> header{};
  std::memcpy(header.data(), kMagic, sizeof(kMagic));
  fr::writeLe16(header.data() + 4, kVersion);
  fr::writeLe16(header.data() + 6, static_cast<uint16_t>(featureDim_));
  fr::writeLe32(header.data() + 8, static_cast<uint32_t>(maxUsers_));
  fr::writeLe32(header.data() + 12, static_cast<uint32_t>(users.size()));

  uint32_t templateOffset = static_cast<uint32_t>(kHeaderSize);
  for (const auto &u : users) {
    const uint32_t perTemplateBytes = 4u + static_cast<uint32_t>(featureDim_ * 4);
    templateOffset += static_cast<uint32_t>(u.templates.size()) * perTemplateBytes;
  }
  const uint32_t indexOffset = templateOffset;
  fr::writeLe32(header.data() + 16, indexOffset);

  if (!storage_.writeAt(tmpPath_.c_str(), 0, header.data(), header.size())) {
    return false;
  }

  uint32_t writeOffset = static_cast<uint32_t>(kHeaderSize);
  std::vector<UserData> materializedUsers = users;

  for (auto &u : materializedUsers) {
    u.firstTemplateOffset = writeOffset;
    for (size_t i = 0; i < u.templates.size(); ++i) {
      std::vector<uint8_t> rec(4 + static_cast<size_t>(featureDim_) * 4u, 0);
      rec[0] = static_cast<uint8_t>(i & 0xFFu);
      for (int d = 0; d < featureDim_; ++d) {
        const float v = u.templates[i][static_cast<size_t>(d)];
        writeFloatLe(rec.data() + 4 + d * 4, v);
      }
      if (!storage_.writeAt(tmpPath_.c_str(), writeOffset, rec.data(), rec.size())) {
        return false;
      }
      writeOffset += static_cast<uint32_t>(rec.size());
    }
  }

  uint32_t idxWriteOffset = indexOffset;
  for (const auto &u : materializedUsers) {
    std::array<uint8_t, kIndexEntrySize> idx{};
    fr::writeLe32(idx.data() + 0, u.userId);
    fr::writeLe32(idx.data() + 4, u.timestamp);
    fr::writeLe32(idx.data() + 8, u.firstTemplateOffset);
    fr::writeLe16(idx.data() + 12, static_cast<uint16_t>(u.templates.size()));
    if (!storage_.writeAt(tmpPath_.c_str(), idxWriteOffset, idx.data(), idx.size())) {
      return false;
    }
    idxWriteOffset += static_cast<uint32_t>(idx.size());
  }

  if (storage_.fileExists(dbPath_.c_str())) {
    storage_.rename(dbPath_.c_str(), bakPath_.c_str());
  }
  if (!storage_.rename(tmpPath_.c_str(), dbPath_.c_str())) {
    return false;
  }
  return true;
}

void FaceDatabase::rebuildCentroidsLocked(std::vector<UserData> &users) const {
  for (auto &u : users) {
    u.centroid = centroidFromTemplates(u.templates, featureDim_);
  }
}