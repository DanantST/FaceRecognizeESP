#pragma once

#include <stdint.h>
#include <vector>
#include <utility>

// ImageFrame is intentionally simple and camera-pipeline agnostic.
// The library expects face-cropped ROI frames.
struct ImageFrame {
  int width = 0;
  int height = 0;
  int channels = 1;
  int stride = 0;  // bytes per row; 0 means tightly packed
  std::vector<uint8_t> pixels;
  uint32_t timestampMs = 0;

  ImageFrame() = default;

  ImageFrame(int w, int h, int c, std::vector<uint8_t> data, uint32_t ts = 0)
      : width(w), height(h), channels(c), stride(w * c), pixels(std::move(data)), timestampMs(ts) {}

  bool valid() const {
    if (width <= 0 || height <= 0 || channels <= 0) {
      return false;
    }
    const int expected = (stride > 0 ? stride : width * channels) * height;
    return static_cast<int>(pixels.size()) >= expected;
  }
};