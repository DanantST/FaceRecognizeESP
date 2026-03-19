#pragma once

#include "../include/ImageFrame.h"

#include <array>
#include <vector>

namespace FaceRecognize {

class FeatureExtractor {
 public:
  FeatureExtractor(int faceSize, int featureDim);

  bool extract(const ImageFrame &frame, std::vector<float> &outFeature) const;

  static void computeLbpHistogram(const uint8_t *gray,
                                  int width,
                                  int height,
                                  std::array<float, 256> &hist);
  static std::vector<float> projectHistogram(const std::array<float, 256> &hist,
                                             int featureDim);

 private:
  static void l2Normalize(std::vector<float> &v);

  int faceSize_;
  int featureDim_;
};

} // namespace FaceRecognize