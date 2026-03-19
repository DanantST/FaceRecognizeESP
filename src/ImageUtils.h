#pragma once

#include "../include/ImageFrame.h"

#include <vector>

namespace FaceRecognize {
    struct ImageFrame;
}

namespace fr {
    
std::vector<uint8_t> convertToGrayscale(const FaceRecognize::ImageFrame &frame);
std::vector<uint8_t> resizeBilinearGray(const uint8_t *src, int srcW, int srcH, int dstW, int dstH);
float varianceOfLaplacian(const uint8_t *gray, int width, int height);

uint32_t millis();

}  // namespace fr