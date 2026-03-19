#include "FeatureExtractor.h"

#include "../include/pca_params.h"
#include "ImageUtils.h"

#include <algorithm>
#include <cmath>

namespace FaceRecognize {

FeatureExtractor::FeatureExtractor(int faceSize, int featureDim)
    : faceSize_(faceSize), featureDim_(featureDim) {}

bool FeatureExtractor::extract(const ImageFrame &frame, std::vector<float> &outFeature) const {
  if (frame.pixels.empty()) return false;

  // 1. Compute LBP Histogram
  std::array<float, 256> hist;
  computeLbpHistogram(frame.pixels.data(), frame.width, frame.height, hist);
  
  // 2. Project using PCA Matrix
  outFeature = projectHistogram(hist, featureDim_);
  
  // 3. L2 Normalization (Unit Length)
  l2Normalize(outFeature);
  
  return true;
}

void FeatureExtractor::computeLbpHistogram(const uint8_t *gray,
                                            int width,
                                            int height,
                                            std::array<float, 256> &hist) {
  hist.fill(0.0f);
  
  // Basic 3x3 LBP
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      uint8_t center = gray[y * width + x];
      uint8_t code = 0;
      // Clockwise start from top-left
      if (gray[(y - 1) * width + (x - 1)] >= center) code |= (1 << 7);
      if (gray[(y - 1) * width + x]       >= center) code |= (1 << 6);
      if (gray[(y - 1) * width + (x + 1)] >= center) code |= (1 << 5);
      if (gray[y * width + (x + 1)]       >= center) code |= (1 << 4);
      if (gray[(y + 1) * width + (x + 1)] >= center) code |= (1 << 3);
      if (gray[(y + 1) * width + x]       >= center) code |= (1 << 2);
      if (gray[(y + 1) * width + (x - 1)] >= center) code |= (1 << 1);
      if (gray[y * width + (x - 1)]       >= center) code |= (1 << 0);
      hist[code] += 1.0f;
    }
  }
  
  // L1 Normalization
  float sum = 0;
  for (float v : hist) sum += v;
  if (sum > 0) {
    for (float &v : hist) v /= sum;
  }
}

std::vector<float> FeatureExtractor::projectHistogram(const std::array<float, 256> &hist,
                                                       int featureDim) {
  std::vector<float> proj(featureDim, 0.0f);
  for (int i = 0; i < featureDim; ++i) {
    float sum = 0;
    for (int j = 0; j < 256; ++j) {
      sum += (hist[j] - LBP_MEAN[j]) * PCA_MATRIX[i][j];
    }
    proj[i] = sum;
  }
  return proj;
}

void FeatureExtractor::l2Normalize(std::vector<float> &v) {
  float sqSum = 0;
  for (float val : v) sqSum += val * val;
  float norm = std::sqrt(sqSum);
  if (norm > 1e-6f) {
    for (float &val : v) val /= norm;
  }
}

} // namespace FaceRecognize