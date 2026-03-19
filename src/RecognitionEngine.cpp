#include "RecognitionEngine.h"
#include "Utils.h"
#include "Logger.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace FaceRecognize {

RecognitionEngine::RecognitionEngine(FeatureExtractor &extractor,
                                     FaceDatabase &database,
                                     Logger &logger,
                                     const Config &cfg)
    : extractor_(extractor), database_(database), logger_(logger), cfg_(cfg) {}

bool RecognitionEngine::begin() {
  std::lock_guard<std::mutex> lock(mu_);
  if (running_) return true;
  running_ = true;
  worker_ = std::thread(&RecognitionEngine::workerLoop, this);
  return true;
}

void RecognitionEngine::shutdown() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!running_) return;
    running_ = false;
    cv_.notify_all();
  }
  if (worker_.joinable()) {
    worker_.join();
  }
}

RecognitionResult RecognitionEngine::recognize(const ImageFrame &frame) {
  if (!frame.valid()) return {-1, 0.0f, ""};

  std::vector<float> feat;
  if (!extractor_.extract(frame, feat)) {
    return {-1, 0.0f, ""};
  }

  logger_.log("Accessing Database for Recognition...");
  
  // Single frame 1:N matching
  FaceDatabase::SearchResult res = database_.search(feat, cfg_.topK);
  
  if (res.userId != -1 && res.similarity >= cfg_.matchThreshold) {
      return {res.userId, res.similarity, LibString(res.name.c_str())};
  }

  return {-1, res.similarity, ""};
}

RecognitionResult RecognitionEngine::recognizeTwoFrames(const ImageFrame &faceRoi1, const ImageFrame &faceRoi2) {
    logger_.log("Processing Recognition (Two Frames)...");
    
    RecognitionResult res1 = recognize(faceRoi1);
    RecognitionResult res2 = recognize(faceRoi2);

    if (res1.id != -1 && res2.id != -1 && res1.id == res2.id) {
        // Confirmed match on both frames
        return {res1.id, (res1.similarity + res2.similarity) / 2.0f, res1.name};
    } else if (res1.id != -1 && res1.similarity > res2.similarity) {
        return res1;
    } else {
        return res2;
    }
}

void RecognitionEngine::setMatchThreshold(float threshold) {
    cfg_.matchThreshold = threshold;
}

void RecognitionEngine::workerLoop() {
  while (true) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [this] { return !running_ || !jobs_.empty(); });
      if (!running_ && jobs_.empty()) break;
      job = std::move(jobs_.front());
      jobs_.pop();
    }
    
    job.promise.set_value(process(job.frame1, job.frame2));
  }
}

RecognitionResult RecognitionEngine::process(const ImageFrame &faceRoi1, const ImageFrame &faceRoi2) {
    // This could be the async implementation of recognizeTwoFrames.
    // For now the synchronous version is used by FaceRecognizeESP.
    return recognizeTwoFrames(faceRoi1, faceRoi2);
}

float RecognitionEngine::cosineSimilarity(const std::vector<float> &a, const std::vector<float> &b) {
  if (a.size() != b.size() || a.empty()) return 0.0f;
  float dot = 0.0f, normA = 0.0f, normB = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    dot += a[i] * b[i];
    normA += a[i] * a[i];
    normB += b[i] * b[i];
  }
  if (normA <= 0.0f || normB <= 0.0f) return 0.0f;
  return dot / (std::sqrt(normA) * std::sqrt(normB));
}

} // namespace FaceRecognize