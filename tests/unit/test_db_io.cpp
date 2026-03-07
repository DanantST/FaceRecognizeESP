#include "FaceDatabase.h"
#include "../test_common.h"
#include "../../adapters/StorageAdapter.h"

#include <array>
#include <vector>

int main() {
  int failures = 0;

  FakeStorageAdapter storage;
  failures += expectTrue(storage.begin(), "Fake storage begin should succeed");

  FaceDatabase db(storage, 4, 10, 5, true);
  failures += expectTrue(db.begin(), "Database begin should succeed");

  std::vector<std::vector<float>> templates(5, std::vector<float>{0.1f, 0.2f, 0.3f, 0.4f});
  templates[1] = {0.11f, 0.21f, 0.31f, 0.41f};
  templates[2] = {0.12f, 0.22f, 0.32f, 0.42f};
  templates[3] = {0.13f, 0.23f, 0.33f, 0.43f};
  templates[4] = {0.14f, 0.24f, 0.34f, 0.44f};

  std::vector<float> centroid{0.12f, 0.22f, 0.32f, 0.42f};
  uint32_t timestamp = 0;

  failures += expectTrue(db.enrollUser(7, templates, centroid, timestamp), "Enrollment should succeed");
  failures += expectTrue(timestamp > 0, "Enrollment timestamp should be populated");
  failures += expectTrue(db.getNumUsers() == 1, "Database should contain one user");

  std::vector<std::vector<float>> loaded;
  failures += expectTrue(db.loadUserTemplates(7, loaded), "Should load enrolled templates");
  failures += expectTrue(loaded.size() == templates.size(), "Template count should match");

  for (size_t i = 0; i < templates.size() && i < loaded.size(); ++i) {
    for (size_t d = 0; d < templates[i].size() && d < loaded[i].size(); ++d) {
      failures += expectNear(loaded[i][d], templates[i][d], 1e-6f, "Loaded template value mismatch");
    }
  }

  std::array<uint8_t, 4> magic{};
  failures += expectTrue(storage.readAt("/face_db.bin", 0, magic.data(), magic.size()), "Database file should exist");
  failures += expectTrue(magic[0] == 'F' && magic[1] == 'R' && magic[2] == 'E' && magic[3] == 'C',
                         "Database magic should match");

  db.shutdown();
  storage.end();
  return failures == 0 ? 0 : 1;
}