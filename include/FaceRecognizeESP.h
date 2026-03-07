#pragma once

#include <stdint.h>
#include <vector>

#include "ImageFrame.h"

#if !defined(ARDUINO)
#include <string>
using String = std::string;
#endif

struct RecognitionResult {
  bool matched;
  uint32_t userId;     // valid if matched==true
  float confidence;    // [0.0 .. 1.0]
};

class FaceRecognizeESP {
 public:
  struct Config {
    int featureDim = 100;           // default PCA output dims
    int faceSize = 96;              // ROI normalized size
    int maxUsers = 500;
    int templatesPerUser = 5;       // fixed
    int topK = 5;                   // centroid filter candidate count
    float matchThreshold = 0.75f;   // normalized similarity threshold (cosine)
    bool loadCentroidsToRam = true;
  };

  FaceRecognizeESP(const Config &cfg = Config());

  // Initialize; must be called before other APIs. Returns false if fatal error.
  bool begin();

  // Toggle SD storage (one-liner). If true and SD present, DB stored on SD. If false -> SPIFFS.
  void enableSDStorage(bool enable);

  // Enroll a user with exactly config.templatesPerUser images (poses).
  // Returns true on success.
  bool enrollUser(uint32_t userId, const std::vector<ImageFrame> &faceRois, uint32_t &outTimestamp);

  // Recognize using two face ROIs captured 1 second apart.
  // Non-blocking variant: scheduleRecognition() spawns background task. But the simple blocking API:
  RecognitionResult recognizeTwoFrames(const ImageFrame &faceRoi1, const ImageFrame &faceRoi2);

  // Diagnostics and DB stats
  String diagnostics();
  int getNumUsers();
  void shutdown();

 private:
  struct Impl;
  Impl *impl_;
};