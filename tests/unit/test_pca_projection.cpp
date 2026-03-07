#include "FeatureExtractor.h"
#include "../test_common.h"

#include <array>

int main() {
  int failures = 0;

  std::array<float, 256> hist{};
  hist.fill(0.0f);
  hist[0] = 1.0f;

  const auto proj = FeatureExtractor::projectHistogram(hist, 4);
  failures += expectTrue(proj.size() == 4, "Projected vector length should match feature dim");
  failures += expectNear(proj[0], 1.0f, 1e-4f, "First projection component should be 1");
  failures += expectNear(proj[1], 0.0f, 1e-4f, "Second projection component should be 0");
  failures += expectNear(proj[2], 0.0f, 1e-4f, "Third projection component should be 0");
  failures += expectNear(proj[3], 0.0f, 1e-4f, "Fourth projection component should be 0");

  return failures == 0 ? 0 : 1;
}