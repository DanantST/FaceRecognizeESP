#include "FaceRecognizeESP.h"

#include "../adapters/StorageAdapter.h"
#include "FaceDatabase.h"
#include "FeatureExtractor.h"
#include "ImageUtils.h"
#include "Logger.h"
#include "RecognitionEngine.h"
#include "Utils.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <sstream>

namespace {
constexpr float kBlurThreshold = 15.0f;
}

struct FaceRecognizeESP::Impl {
  explicit Impl(const Config &config) : cfg(config) {}

  Config cfg;
  bool useSdStorage = false;
  bool started = false;

  std::unique_ptr<SPIFFSStorageAdapter> spiffs;
  std::unique_ptr<SDStorageAdapter> sd;
  StorageAdapter *activeStorage = nullptr;

  Logger logger;
  std::unique_ptr<FeatureExtractor> extractor;
  std::unique_ptr<FaceDatabase> database;
  std::unique_ptr<RecognitionEngine> engine;

  std::mutex mu;
};

FaceRecognizeESP::FaceRecognizeESP(const Config &cfg) : impl_(new Impl(cfg)) {}

FaceRecognizeESP::~FaceRecognizeESP() {
  if (impl_ != nullptr) {
    shutdown();
    delete impl_;
    impl_ = nullptr;
  }
}



bool FaceRecognizeESP::begin() {
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (impl_->started) {
    return true;
  }

  impl_->spiffs = std::make_unique<SPIFFSStorageAdapter>();
  if (!impl_->spiffs->begin()) {
    return false;
  }

  impl_->activeStorage = impl_->spiffs.get();

  if (impl_->useSdStorage) {
    impl_->sd = std::make_unique<SDStorageAdapter>();
    if (impl_->sd->begin()) {
      impl_->activeStorage = impl_->sd.get();
    }
  }

  impl_->logger.setStorageAdapter(impl_->activeStorage);
  impl_->logger.begin(impl_->useSdStorage && impl_->activeStorage == impl_->sd.get());

  if (impl_->useSdStorage && impl_->activeStorage != impl_->sd.get()) {
    impl_->logger.log("SD init failed, falling back to SPIFFS");
  }

  impl_->extractor = std::make_unique<FeatureExtractor>(impl_->cfg.faceSize, impl_->cfg.featureDim);
  impl_->database = std::make_unique<FaceDatabase>(*impl_->activeStorage,
                                                   impl_->cfg.featureDim,
                                                   impl_->cfg.maxUsers,
                                                   impl_->cfg.templatesPerUser,
                                                   impl_->cfg.loadCentroidsToRam);
  if (!impl_->database->begin()) {
    return false;
  }

  RecognitionEngine::Config rcfg;
  rcfg.topK = impl_->cfg.topK;
  rcfg.matchThreshold = impl_->cfg.matchThreshold;
  impl_->engine = std::make_unique<RecognitionEngine>(
      *impl_->extractor, *impl_->database, impl_->logger, rcfg);
  if (!impl_->engine->begin()) {
    return false;
  }

  impl_->logger.setRecognitionEngineStartTime(fr::millis());
  impl_->started = true;
  return true;
}

void FaceRecognizeESP::enableSDStorage(bool enable) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  impl_->useSdStorage = enable;
}

bool FaceRecognizeESP::enrollUser(uint32_t userId,
                                  const std::vector<ImageFrame> &faceRois,
                                  uint32_t &outTimestamp) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (!impl_->started || !impl_->extractor || !impl_->database) {
    return false;
  }
  if (faceRois.size() != static_cast<size_t>(impl_->cfg.templatesPerUser)) {
    return false;
  }

  std::vector<std::vector<float>> templates;
  templates.reserve(faceRois.size());

  for (size_t i = 0; i < faceRois.size(); ++i) {
    impl_->logger.log("Frame received");
    impl_->logger.log("Running detection");
    if (!faceRois[i].valid()) {
      impl_->logger.log("No face detected");
      return false;
    }
    impl_->logger.log("Face detected");
    impl_->logger.log("Image captured %d", static_cast<int>(i + 1));

    std::vector<uint8_t> gray = fr::toGrayscale(faceRois[i]);
    if (gray.empty()) {
      return false;
    }

    std::vector<uint8_t> resized;
    if (faceRois[i].width == impl_->cfg.faceSize && faceRois[i].height == impl_->cfg.faceSize) {
      resized = gray;
    } else {
      resized = fr::resizeBilinearGray(
          gray.data(), faceRois[i].width, faceRois[i].height, impl_->cfg.faceSize, impl_->cfg.faceSize);
    }

    if (resized.empty()) {
      return false;
    }

    if (fr::varianceOfLaplacian(resized.data(), impl_->cfg.faceSize, impl_->cfg.faceSize) < kBlurThreshold) {
      return false;
    }

    std::vector<float> feat;
    if (!impl_->extractor->extract(faceRois[i], feat)) {
      return false;
    }
    templates.push_back(std::move(feat));
  }

  std::vector<float> centroid(static_cast<size_t>(impl_->cfg.featureDim), 0.0f);
  for (const auto &t : templates) {
    for (int d = 0; d < impl_->cfg.featureDim; ++d) {
      centroid[static_cast<size_t>(d)] += t[static_cast<size_t>(d)];
    }
  }
  const float invCount = 1.0f / static_cast<float>(templates.size());
  for (float &v : centroid) {
    v *= invCount;
  }

  return impl_->database->enrollUser(userId, templates, centroid, outTimestamp);
}

RecognitionResult FaceRecognizeESP::recognizeTwoFrames(const ImageFrame &faceRoi1,
                                                        const ImageFrame &faceRoi2) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (!impl_->started || !impl_->engine) {
    return {false, 0, 0.0f};
  }

  impl_->logger.log("Frame received");
  impl_->logger.log("Running detection");
  if (!faceRoi1.valid() || !faceRoi2.valid()) {
    impl_->logger.log("No face detected");
    return {false, 0, 0.0f};
  }
  impl_->logger.log("Face detected");

  impl_->logger.log("Image captured 1");
  impl_->logger.log("Image captured 2");
  return impl_->engine->recognizeTwoFrames(faceRoi1, faceRoi2);
}

String FaceRecognizeESP::diagnostics() {
  std::lock_guard<std::mutex> lock(impl_->mu);
  std::ostringstream oss;
  oss << "started=" << (impl_->started ? "true" : "false")
      << ";storage=" << (impl_->activeStorage == impl_->sd.get() ? "SD" : "SPIFFS")
      << ";feature_dim=" << impl_->cfg.featureDim
      << ";face_size=" << impl_->cfg.faceSize;
  if (impl_->database) {
    oss << ";" << impl_->database->diagnostics();
  }
  return oss.str();
}

int FaceRecognizeESP::getNumUsers() {
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (!impl_->database) {
    return 0;
  }
  return impl_->database->getNumUsers();
}

void FaceRecognizeESP::shutdown() {
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (impl_->engine) {
    impl_->engine->shutdown();
    impl_->engine.reset();
  }
  if (impl_->database) {
    impl_->database->shutdown();
    impl_->database.reset();
  }
  impl_->extractor.reset();
  impl_->logger.shutdown();
  impl_->sd.reset();
  impl_->spiffs.reset();
  impl_->activeStorage = nullptr;
  impl_->started = false;
}
