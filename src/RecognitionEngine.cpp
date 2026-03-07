#include "RecognitionEngine.h"

#include "Utils.h"

#include <algorithm>
#include <cmath>

RecognitionEngine::RecognitionEngine(FeatureExtractor &extractor,
                                     FaceDatabase &database,
                                     Logger &logger,
                                     const Config &cfg)
    : extractor_(extractor), database_(database), logger_(logger), cfg_(cfg) {}

bool RecognitionEngine::begin() {
  std::lock_guard<std::mutex> lock(mu_);
  if (running_) {
    return true;
  }
  running_ = true;
  worker_ = std::thread(&RecognitionEngine::workerLoop, this);
  return true;
}

void RecognitionEngine::shutdown() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!running_) {
      return;
    }
    running_ = false;
  }
  cv_.notify_all();
  if (worker_.joinable()) {
    worker_.join();
  }
}

RecognitionResult RecognitionEngine::recognizeTwoFrames(const ImageFrame &faceRoi1,
                                                        const ImageFrame &faceRoi2) {
  Job job;
  job.frame1 = faceRoi1;
  job.frame2 = faceRoi2;
  std::future<RecognitionResult> result = job.promise.get_future();

  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!running_) {
      return {false, 0, 0.0f};
    }
    jobs_.push(std::move(job));
  }
  cv_.notify_one();
  return result.get();
}

void RecognitionEngine::setMatchThreshold(float threshold) {
  std::lock_guard<std::mutex> lock(mu_);
  cfg_.matchThreshold = threshold;
}

void RecognitionEngine::workerLoop() {
  while (true) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [&]() { return !running_ || !jobs_.empty(); });
      if (!running_ && jobs_.empty()) {
        break;
      }
      job = std::move(jobs_.front());
      jobs_.pop();
    }

    RecognitionResult result = process(job.frame1, job.frame2);
    job.promise.set_value(result);
    fr::yieldTask();
  }
}

RecognitionResult RecognitionEngine::process(const ImageFrame &faceRoi1, const ImageFrame &faceRoi2) {
  logger_.log("Processing Recognition");

  std::vector<float> feat1;
  std::vector<float> feat2;
  if (!extractor_.extract(faceRoi1, feat1) || !extractor_.extract(faceRoi2, feat2)) {
    logger_.log("Not Recognized");
    return {false, 0, 0.0f};
  }

  logger_.log("Accessing Database");

  std::vector<std::pair<uint32_t, std::vector<float>>> centroids = database_.getCentroids();
  if (centroids.empty()) {
    logger_.log("Found %d similar data", 0);
    logger_.log("Not Recognized");
    return {false, 0, 0.0f};
  }

  std::vector<float> avg(feat1.size(), 0.0f);
  for (size_t i = 0; i < avg.size(); ++i) {
    avg[i] = 0.5f * (feat1[i] + feat2[i]);
  }

  std::vector<std::pair<uint32_t, float>> ranked;
  ranked.reserve(centroids.size());
  for (const auto &entry : centroids) {
    const float sim = cosineSimilarity(avg, entry.second);
    ranked.push_back({entry.first, sim});
  }

  std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) {
    return a.second > b.second;
  });

  if (ranked.size() > static_cast<size_t>(cfg_.topK)) {
    ranked.resize(static_cast<size_t>(cfg_.topK));
  }

  logger_.log("Found %d similar data", static_cast<int>(ranked.size()));

  uint32_t bestUser = 0;
  float bestScore = -1.0f;

  for (const auto &candidate : ranked) {
    logger_.log("Calculating weight points");
    std::vector<std::vector<float>> templates;
    if (!database_.loadUserTemplates(candidate.first, templates)) {
      continue;
    }

    float d1 = -1.0f;
    float d2 = -1.0f;
    for (const auto &templ : templates) {
      d1 = std::max(d1, cosineSimilarity(feat1, templ));
      d2 = std::max(d2, cosineSimilarity(feat2, templ));
    }
    const float combined = 0.5f * d1 + 0.5f * d2;
    if (combined > bestScore) {
      bestScore = combined;
      bestUser = candidate.first;
    }
  }

  const float threshold = [&]() {
    std::lock_guard<std::mutex> lock(mu_);
    return cfg_.matchThreshold;
  }();

  if (bestScore >= threshold) {
    logger_.log("Recognized: %u (conf=%0.3f)", bestUser, bestScore);
    return {true, bestUser, bestScore};
  }

  logger_.log("Not Recognized");
  return {false, 0, std::max(0.0f, bestScore)};
}

float RecognitionEngine::cosineSimilarity(const std::vector<float> &a, const std::vector<float> &b) {
  if (a.size() != b.size() || a.empty()) {
    return -1.0f;
  }
  float dot = 0.0f;
  float na = 0.0f;
  float nb = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    dot += a[i] * b[i];
    na += a[i] * a[i];
    nb += b[i] * b[i];
  }
  if (na <= 1e-12f || nb <= 1e-12f) {
    return -1.0f;
  }
  return dot / (std::sqrt(na) * std::sqrt(nb));
}