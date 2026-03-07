#include "FeatureExtractor.h"
#include "../test_common.h"

#include <array>
#include <cstdint>

int main() {
  int failures = 0;

  // Constant image: all neighbors are >= center for each valid inner pixel.
  std::array<uint8_t, 25> img{};
  img.fill(42);

  std::array<float, 256> hist{};
  FeatureExtractor::computeLbpHistogram(img.data(), 5, 5, hist);

  float sum = 0.0f;
  for (float v : hist) {
    sum += v;
  }

  failures += expectNear(sum, 9.0f, 1e-5f, "LBP histogram count should match inner pixel count");
  failures += expectNear(hist[255], 9.0f, 1e-5f, "LBP code 255 should dominate for constant image");

  if (failures == 0) {
    return 0;
  }
  return 1;
}