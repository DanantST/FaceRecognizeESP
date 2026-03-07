#pragma once

#include "../adapters/StorageAdapter.h"

#include <mutex>
#include <string>
#include <utility>
#include <vector>

class FaceDatabase {
 public:
  FaceDatabase(StorageAdapter &storage,
               int featureDim,
               int maxUsers,
               int templatesPerUser,
               bool loadCentroidsToRam,
               const std::string &dbPath = "/face_db.bin");

  bool begin();
  void shutdown();

  bool enrollUser(uint32_t userId,
                  const std::vector<std::vector<float>> &templates,
                  const std::vector<float> &centroid,
                  uint32_t &outTimestamp);

  int getNumUsers() const;
  std::vector<std::pair<uint32_t, std::vector<float>>> getCentroids() const;
  bool loadUserTemplates(uint32_t userId, std::vector<std::vector<float>> &outTemplates) const;
  std::string diagnostics() const;

 private:
  struct UserData {
    uint32_t userId = 0;
    uint32_t timestamp = 0;
    uint32_t firstTemplateOffset = 0;
    std::vector<std::vector<float>> templates;
    std::vector<float> centroid;
  };

  bool loadDbLocked(const std::string &path, std::vector<UserData> &outUsers);
  bool writeDbLocked(const std::vector<UserData> &users);
  void rebuildCentroidsLocked(std::vector<UserData> &users) const;

  StorageAdapter &storage_;
  const int featureDim_;
  const int maxUsers_;
  const int templatesPerUser_;
  const bool loadCentroidsToRam_;
  const std::string dbPath_;
  const std::string tmpPath_;
  const std::string bakPath_;

  mutable std::mutex mu_;
  std::vector<UserData> users_;
  bool started_ = false;
};