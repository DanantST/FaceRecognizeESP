#pragma once

#include "../include/FaceRecognizeESP.h"
#include "FaceDatabase.h"
#include "FeatureExtractor.h"
#include "Logger.h"

#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

class RecognitionEngine {
 public:
  struct Config {
    int topK = 5;
    float matchThreshold = 0.75f;
  };

  RecognitionEngine(FeatureExtractor &extractor,
                    FaceDatabase &database,
                    Logger &logger,
                    const Config &cfg = Config{});

  bool begin();
  void shutdown();
  RecognitionResult recognizeTwoFrames(const ImageFrame &faceRoi1, const ImageFrame &faceRoi2);
  void setMatchThreshold(float threshold);

 private:
  struct Job {
    ImageFrame frame1;
    ImageFrame frame2;
    std::promise<RecognitionResult> promise;
  };

  void workerLoop();
  RecognitionResult process(const ImageFrame &faceRoi1, const ImageFrame &faceRoi2);
  static float cosineSimilarity(const std::vector<float> &a, const std::vector<float> &b);

  FeatureExtractor &extractor_;
  FaceDatabase &database_;
  Logger &logger_;
  Config cfg_;

  std::mutex mu_;
  std::condition_variable cv_;
  std::queue<Job> jobs_;
  std::thread worker_;
  bool running_ = false;
};