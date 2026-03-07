#include "../test_common.h"

#include "../../adapters/StorageAdapter.h"
#include "../../src/FaceDatabase.h"
#include "../../src/FeatureExtractor.h"
#include "../../src/Logger.h"
#include "../../src/RecognitionEngine.h"
#include "../../src/Utils.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

struct FakeCameraAdapter {
  static ImageFrame makeGradientFrame(int w, int h, uint8_t seed, uint32_t ts) {
    std::vector<uint8_t> px(static_cast<size_t>(w * h));
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        px[static_cast<size_t>(y * w + x)] = static_cast<uint8_t>((x + y + seed) % 255);
      }
    }
    return ImageFrame(w, h, 1, std::move(px), ts);
  }
};

std::string readAll(StorageAdapter &storage, const char *path) {
  std::string out;
  uint8_t byte = 0;
  uint32_t offset = 0;
  while (storage.readAt(path, offset, &byte, 1)) {
    out.push_back(static_cast<char>(byte));
    ++offset;
  }
  return out;
}

std::vector<float> average(const std::vector<std::vector<float>> &vectors) {
  if (vectors.empty()) {
    return {};
  }
  std::vector<float> out(vectors[0].size(), 0.0f);
  for (const auto &v : vectors) {
    for (size_t i = 0; i < v.size(); ++i) {
      out[i] += v[i];
    }
  }
  const float inv = 1.0f / static_cast<float>(vectors.size());
  for (float &x : out) {
    x *= inv;
  }
  return out;
}

}  // namespace

int main() {
  int failures = 0;

  FakeStorageAdapter dbStorage;
  FakeStorageAdapter fakeSdStorage;
  failures += expectTrue(dbStorage.begin(), "DB storage begin should succeed");
  failures += expectTrue(fakeSdStorage.begin(), "SD storage begin should succeed");

  FeatureExtractor extractor(24, 8);
  FaceDatabase database(dbStorage, 8, 100, 5, true);
  failures += expectTrue(database.begin(), "Database begin should succeed");

  std::vector<ImageFrame> enrollmentFrames;
  for (int i = 0; i < 5; ++i) {
    enrollmentFrames.push_back(FakeCameraAdapter::makeGradientFrame(24, 24, static_cast<uint8_t>(10 + i), fr::millis()));
  }

  std::vector<std::vector<float>> templates;
  for (const auto &f : enrollmentFrames) {
    std::vector<float> feat;
    failures += expectTrue(extractor.extract(f, feat), "Feature extraction during enrollment should succeed");
    templates.push_back(std::move(feat));
  }

  uint32_t ts = 0;
  failures += expectTrue(database.enrollUser(42, templates, average(templates), ts), "Enroll user into DB should succeed");

  Logger logger;
  logger.setStorageAdapter(&fakeSdStorage);
  logger.begin(true);
  logger.setRecognitionEngineStartTime(fr::millis() - 2500);

  RecognitionEngine::Config cfg;
  cfg.topK = 5;
  cfg.matchThreshold = 0.15f;
  RecognitionEngine engine(extractor, database, logger, cfg);
  failures += expectTrue(engine.begin(), "Recognition engine should start");

  const ImageFrame probe1 = FakeCameraAdapter::makeGradientFrame(24, 24, 11, fr::millis());
  const ImageFrame probe2 = FakeCameraAdapter::makeGradientFrame(24, 24, 12, fr::millis() + 1000);

  const RecognitionResult result = engine.recognizeTwoFrames(probe1, probe2);
  failures += expectTrue(result.matched, "Recognition should match");
  failures += expectTrue(result.userId == 42, "Recognized user should be enrolled user");

  const std::string sdLog = readAll(fakeSdStorage, "/logs/faceengine.log");
  failures += expectTrue(!sdLog.empty(), "SD log should be written after 2s");
  failures += expectTrue(sdLog.find("Processing Recognition") != std::string::npos,
                         "SD log should include processing marker");
  failures += expectTrue(sdLog.find("Accessing Database") != std::string::npos,
                         "SD log should include DB marker");

  engine.shutdown();
  database.shutdown();
  dbStorage.end();
  fakeSdStorage.end();

  return failures == 0 ? 0 : 1;
}