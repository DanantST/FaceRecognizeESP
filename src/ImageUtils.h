#pragma once

#include "../include/ImageFrame.h"

#include <vector>

namespace fr {

std::vector<uint8_t> toGrayscale(const ImageFrame &frame);
std::vector<uint8_t> resizeBilinearGray(const uint8_t *src, int srcW, int srcH, int dstW, int dstH);
float varianceOfLaplacian(const uint8_t *gray, int width, int height);

}  // namespace fr