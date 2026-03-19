#include "FaceDatabase.h"
#include "Utils.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace FaceRecognize {

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
      dbPath_(dbPath) {}

bool FaceDatabase::begin() {
    std::lock_guard<std::mutex> lock(mu_);
    if (started_) return true;
    
    // Mock loading logic for now or simple binary read if storage is SPIFFS/SD
    // For this build, we start with an empty database in memory
    started_ = true;
    return true;
}

void FaceDatabase::shutdown() {
    std::lock_guard<std::mutex> lock(mu_);
    started_ = false;
}

bool FaceDatabase::enrollUser(uint32_t userId,
                              const std::vector<std::vector<float>> &templates,
                              const std::vector<float> &centroid,
                              uint32_t &outTimestamp,
                              const std::string &name) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!started_) return false;
    
    if (users_.size() >= static_cast<size_t>(maxUsers_)) return false;

    UserData user;
    user.userId = userId;
    user.name = name;
    user.timestamp = fr::millis();
    user.templates = templates;
    user.centroid = centroid;
    
    users_.push_back(std::move(user));
    outTimestamp = user.timestamp;
    
    return true;
}

FaceDatabase::SearchResult FaceDatabase::search(const std::vector<float> &feature, int topK) const {
    std::lock_guard<std::mutex> lock(mu_);
    if (!started_ || users_.empty()) return {-1, 0.0f, ""};

    int bestId = -1;
    float bestSim = -1.0f;
    std::string bestName = "";

    for (const auto &user : users_) {
        // Compare against centroid
        float sim = 0.0f;
        float dot = 0.0f, normA = 0.0f, normB = 0.0f;
        for (size_t i = 0; i < feature.size() && i < user.centroid.size(); ++i) {
            dot += feature[i] * user.centroid[i];
            normA += feature[i] * feature[i];
            normB += user.centroid[i] * user.centroid[i];
        }
        if (normA > 0 && normB > 0) {
            sim = dot / (std::sqrt(normA) * std::sqrt(normB));
        }

        if (sim > bestSim) {
            bestSim = sim;
            bestId = (int)user.userId;
            bestName = user.name;
        }
    }

    return {bestId, bestSim, bestName};
}

void FaceDatabase::clear() {
    std::lock_guard<std::mutex> lock(mu_);
    users_.clear();
}

bool FaceDatabase::removeUser(uint32_t userId) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = std::remove_if(users_.begin(), users_.end(), [userId](const UserData &u) {
        return u.userId == userId;
    });
    if (it != users_.end()) {
        users_.erase(it, users_.end());
        return true;
    }
    return false;
}

int FaceDatabase::getNumUsers() const {
    std::lock_guard<std::mutex> lock(mu_);
    return static_cast<int>(users_.size());
}

std::vector<std::pair<uint32_t, std::vector<float>>> FaceDatabase::getCentroids() const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::pair<uint32_t, std::vector<float>>> res;
    for (const auto &u : users_) {
        res.push_back({u.userId, u.centroid});
    }
    return res;
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
    return "users=" + std::to_string(users_.size());
}

} // namespace FaceRecognize