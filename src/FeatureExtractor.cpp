#include "FeatureExtractor.h"

#include "../include/pca_params.h"
#include "ImageUtils.h"

#include <algorithm>
#include <cmath>

FeatureExtractor::FeatureExtractor(int faceSize, int featureDim)
    : faceSize_(faceSize), featureDim_(featureDim) {}

void FeatureExtractor::computeLbpHistogram(const uint8_t *gray,
                                           int width,
                                           int height,
                                           std::array<float, 256> &hist) {
  hist.fill(0.0f);
  if (gray == nullptr || width < 3 || height < 3) {
    return;
  }

  for (int y = 1; y < height - 1; ++y) {
    const uint8_t *rowUp = gray + (y - 1) * width;
    const uint8_t *row = gray + y * width;
    const uint8_t *rowDown = gray + (y + 1) * width;

    for (int x = 1; x < width - 1; ++x) {
      const uint8_t c = row[x];
      uint8_t code = 0;
      code |= static_cast<uint8_t>((rowUp[x - 1] >= c) << 7);
      code |= static_cast<uint8_t>((rowUp[x] >= c) << 6);
      code |= static_cast<uint8_t>((rowUp[x + 1] >= c) << 5);
      code |= static_cast<uint8_t>((row[x + 1] >= c) << 4);
      code |= static_cast<uint8_t>((rowDown[x + 1] >= c) << 3);
      code |= static_cast<uint8_t>((rowDown[x] >= c) << 2);
      code |= static_cast<uint8_t>((rowDown[x - 1] >= c) << 1);
      code |= static_cast<uint8_t>((row[x - 1] >= c) << 0);
      hist[code] += 1.0f;
    }
  }
}

std::vector<float> FeatureExtractor::projectHistogram(const std::array<float, 256> &hist,
                                                      int featureDim) {
  const int dim = std::max(1, std::min(featureDim, FEATURE_DIM));

  std::array<float, 256> centered{};
  for (int i = 0; i < 256; ++i) {
    centered[i] = hist[i] - LBP_MEAN[i];
  }

  std::vector<float> feature(static_cast<size_t>(dim), 0.0f);
  for (int j = 0; j < dim; ++j) {
    float acc = 0.0f;
    for (int i = 0; i < 256; ++i) {
      acc += centered[i] * PCA_MATRIX[j][i];
    }
    feature[static_cast<size_t>(j)] = acc;
  }

  l2Normalize(feature);
  return feature;
}

bool FeatureExtractor::extract(const ImageFrame &frame, std::vector<float> &outFeature) const {
  if (!frame.valid()) {
    return false;
  }

  const std::vector<uint8_t> gray = fr::toGrayscale(frame);
  if (gray.empty()) {
    return false;
  }

  std::vector<uint8_t> resized;
  if (frame.width == faceSize_ && frame.height == faceSize_) {
    resized = gray;
  } else {
    resized = fr::resizeBilinearGray(gray.data(), frame.width, frame.height, faceSize_, faceSize_);
  }
  if (resized.empty()) {
    return false;
  }

  std::array<float, 256> hist{};
  computeLbpHistogram(resized.data(), faceSize_, faceSize_, hist);

  // L1 normalization as required.
  float sum = 0.0f;
  for (float v : hist) {
    sum += v;
  }
  if (sum <= 0.0f) {
    return false;
  }
  for (float &v : hist) {
    v /= sum;
  }

  outFeature = projectHistogram(hist, featureDim_);
  return !outFeature.empty();
}

void FeatureExtractor::l2Normalize(std::vector<float> &v) {
  float normSq = 0.0f;
  for (float x : v) {
    normSq += x * x;
  }
  if (normSq <= 1e-12f) {
    return;
  }
  const float invNorm = 1.0f / std::sqrt(normSq);
  for (float &x : v) {
    x *= invNorm;
  }
}