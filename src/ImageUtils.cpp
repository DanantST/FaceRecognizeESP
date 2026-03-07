#include "ImageUtils.h"

#include <algorithm>
#include <cmath>

namespace fr {

std::vector<uint8_t> toGrayscale(const ImageFrame &frame) {
  if (!frame.valid()) {
    return {};
  }
  std::vector<uint8_t> gray(static_cast<size_t>(frame.width * frame.height));
  const int stride = frame.stride > 0 ? frame.stride : frame.width * frame.channels;

  if (frame.channels == 1) {
    for (int y = 0; y < frame.height; ++y) {
      const uint8_t *row = frame.pixels.data() + y * stride;
      uint8_t *dst = gray.data() + y * frame.width;
      std::copy(row, row + frame.width, dst);
    }
    return gray;
  }

  for (int y = 0; y < frame.height; ++y) {
    const uint8_t *row = frame.pixels.data() + y * stride;
    uint8_t *dst = gray.data() + y * frame.width;
    for (int x = 0; x < frame.width; ++x) {
      const uint8_t r = row[x * frame.channels + 0];
      const uint8_t g = row[x * frame.channels + 1];
      const uint8_t b = row[x * frame.channels + 2];
      const int lum = static_cast<int>(0.299f * r + 0.587f * g + 0.114f * b);
      dst[x] = static_cast<uint8_t>(std::clamp(lum, 0, 255));
    }
  }
  return gray;
}

std::vector<uint8_t> resizeBilinearGray(const uint8_t *src, int srcW, int srcH, int dstW, int dstH) {
  std::vector<uint8_t> dst(static_cast<size_t>(dstW * dstH), 0);
  if (src == nullptr || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) {
    return {};
  }

  const float scaleX = static_cast<float>(srcW) / static_cast<float>(dstW);
  const float scaleY = static_cast<float>(srcH) / static_cast<float>(dstH);

  for (int y = 0; y < dstH; ++y) {
    const float gy = (y + 0.5f) * scaleY - 0.5f;
    const int y0 = std::clamp(static_cast<int>(std::floor(gy)), 0, srcH - 1);
    const int y1 = std::clamp(y0 + 1, 0, srcH - 1);
    const float wy = gy - static_cast<float>(y0);

    for (int x = 0; x < dstW; ++x) {
      const float gx = (x + 0.5f) * scaleX - 0.5f;
      const int x0 = std::clamp(static_cast<int>(std::floor(gx)), 0, srcW - 1);
      const int x1 = std::clamp(x0 + 1, 0, srcW - 1);
      const float wx = gx - static_cast<float>(x0);

      const float p00 = src[y0 * srcW + x0];
      const float p01 = src[y0 * srcW + x1];
      const float p10 = src[y1 * srcW + x0];
      const float p11 = src[y1 * srcW + x1];

      const float top = p00 + wx * (p01 - p00);
      const float bot = p10 + wx * (p11 - p10);
      const float v = top + wy * (bot - top);
      dst[y * dstW + x] = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(v)), 0, 255));
    }
  }

  return dst;
}

float varianceOfLaplacian(const uint8_t *gray, int width, int height) {
  if (gray == nullptr || width < 3 || height < 3) {
    return 0.0f;
  }

  double sum = 0.0;
  double sumSq = 0.0;
  int count = 0;

  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      const int c = gray[y * width + x];
      const int lap =
          gray[(y - 1) * width + x] + gray[(y + 1) * width + x] +
          gray[y * width + (x - 1)] + gray[y * width + (x + 1)] - 4 * c;
      const double lv = static_cast<double>(lap);
      sum += lv;
      sumSq += lv * lv;
      ++count;
    }
  }

  if (count == 0) {
    return 0.0f;
  }
  const double mean = sum / count;
  return static_cast<float>((sumSq / count) - mean * mean);
}

}  // namespace fr