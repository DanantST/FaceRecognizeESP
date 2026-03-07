#include <FaceRecognizeESP.h>

FaceRecognizeESP recognizer;

static ImageFrame makeSyntheticFace(uint8_t seed) {
  const int w = 96;
  const int h = 96;
  std::vector<uint8_t> px(static_cast<size_t>(w * h));
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      px[static_cast<size_t>(y * w + x)] = static_cast<uint8_t>((x + y + seed) % 255);
    }
  }
  return ImageFrame(w, h, 1, px, millis());
}

void setup() {
  Serial.begin(115200);
  recognizer.enableSDStorage(true);

  if (!recognizer.begin()) {
    Serial.println("Recognizer begin failed");
    return;
  }

  std::vector<ImageFrame> enrollment;
  enrollment.push_back(makeSyntheticFace(1));   // neutral
  enrollment.push_back(makeSyntheticFace(2));   // eyes_closed
  enrollment.push_back(makeSyntheticFace(3));   // smile
  enrollment.push_back(makeSyntheticFace(4));   // frown
  enrollment.push_back(makeSyntheticFace(5));   // tilt

  uint32_t ts = 0;
  if (recognizer.enrollUser(1001, enrollment, ts)) {
    Serial.printf("Enrolled user 1001 at ts=%u\n", ts);
  } else {
    Serial.println("Enrollment failed");
  }
}

void loop() {
  ImageFrame frame1 = makeSyntheticFace(6);
  delay(1000);
  ImageFrame frame2 = makeSyntheticFace(7);

  RecognitionResult result = recognizer.recognizeTwoFrames(frame1, frame2);
  if (result.matched) {
    Serial.printf("Recognized user %u with conf=%.3f\n", result.userId, result.confidence);
  } else {
    Serial.println("Not recognized");
  }

  delay(2000);
}