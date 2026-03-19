#pragma once

#include <stdint.h>
#include <vector>

#include "ImageFrame.h"

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <string>
#endif

namespace FaceRecognize {

#if defined(ARDUINO)
typedef String LibString;
#else
typedef std::string LibString;
#endif

struct RecognitionResult {
  int id;
  float similarity;
  LibString name;
};

class FaceRecognizeESP {
 public:
  struct Config {
    Config() : featureDim(100), faceSize(96), maxUsers(500), templatesPerUser(5),
               topK(5), matchThreshold(0.75f), loadCentroidsToRam(true) {}
    int featureDim;
    int faceSize;
    int maxUsers;
    int templatesPerUser;
    int topK;
    float matchThreshold;
    bool loadCentroidsToRam;
  };

  FaceRecognizeESP(const Config &cfg = Config{});
  virtual ~FaceRecognizeESP();

  // Initialize; must be called before other APIs. Returns false if fatal error.
  bool begin();

  void enableSDStorage(bool enable);
  bool enrollUser(uint32_t userId, const std::vector<ImageFrame> &faceRois, const LibString &name = LibString(""));
  bool registerUser(int id, const LibString &name, const std::vector<ImageFrame> &templates);
  RecognitionResult recognize(const ImageFrame &frame);

  LibString diagnostics();
  int getNumUsers();
  void clearDatabase();
  bool removeUser(int id);
  void shutdown();

 private:
  struct Impl;
  Impl *impl_;
};

} // namespace FaceRecognize