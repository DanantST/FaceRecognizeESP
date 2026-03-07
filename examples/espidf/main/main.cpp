#include "FaceRecognizeESP.h"

extern "C" void app_main(void) {
  FaceRecognizeESP recognizer;
  recognizer.enableSDStorage(true);
  if (!recognizer.begin()) {
    return;
  }

  std::vector<ImageFrame> enrollment;
  for (int i = 0; i < 5; ++i) {
    std::vector<uint8_t> px(96 * 96);
    for (int y = 0; y < 96; ++y) {
      for (int x = 0; x < 96; ++x) {
        px[static_cast<size_t>(y * 96 + x)] = static_cast<uint8_t>((x + y + i) % 255);
      }
    }
    enrollment.emplace_back(96, 96, 1, px, static_cast<uint32_t>(i * 100));
  }

  uint32_t ts = 0;
  recognizer.enrollUser(2001, enrollment, ts);

  ImageFrame f1 = enrollment[0];
  ImageFrame f2 = enrollment[1];
  RecognitionResult r = recognizer.recognizeTwoFrames(f1, f2);
  (void)r;

  recognizer.shutdown();
}