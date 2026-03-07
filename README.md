# FaceRecognizeESP

FaceRecognizeESP is a hardware-agnostic face recognition library for ESP32 platforms.
It provides enrollment, LBP+PCA feature extraction, persistent template storage (5 templates per user), and two-frame recognition fusion.

## Features

- Public API centered on `FaceRecognizeESP` (`begin`, `enrollUser`, `recognizeTwoFrames`, `diagnostics`, `shutdown`)
- Storage toggle with one line: `enableSDStorage(true/false)`
- Atomic DB writes with backup fallback (`face_db.bin`, `face_db.bin.bak`)
- LBP histogram (256 bins) and PCA projection to configurable feature dimension
- Centroid filter (`topK`) + template-level two-frame refinement
- Threaded recognition worker to avoid blocking foreground tasks
- Logger with Serial output and delayed SD logging (starts after 2 seconds)

## Quickstart

```cpp
#include <FaceRecognizeESP.h>

FaceRecognizeESP recognizer;

void setup() {
  recognizer.enableSDStorage(true);
  if (!recognizer.begin()) {
    return;
  }
}
```

## API Reference

`include/FaceRecognizeESP.h` exposes:

- `bool begin()`
- `void enableSDStorage(bool enable)`
- `bool enrollUser(uint32_t userId, const std::vector<ImageFrame>& faceRois, uint32_t& outTimestamp)`
- `RecognitionResult recognizeTwoFrames(const ImageFrame& faceRoi1, const ImageFrame& faceRoi2)`
- `String diagnostics()`
- `int getNumUsers()`
- `void shutdown()`
- `void setMatchThreshold(float threshold)`

## Build (CMake / Host)

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Build for Arduino CLI

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli compile --fqbn esp32:esp32:esp32 examples/arduino/recognize_example
```

## Build for ESP-IDF

```bash
cd examples/espidf
idf.py set-target esp32
idf.py build
```

## PCA Offline Generation

```bash
python3 tools/compute_pca.py --input ./dataset --dim 100 --out include/pca_params.h
python3 tools/test_pca.py --dim 16
```

## Database Format

- File: `/face_db.bin`
- Header: 64 bytes (`FREC`, version, feature dim, max users, num users, index offset)
- Index entries: fixed 32 bytes
- Templates: `poseLabel + padding + float features[FEATURE_DIM]`

## Threshold and Performance Tuning

- `Config::matchThreshold` controls recognition strictness (`0.75` default)
- `Config::topK` controls refinement candidate count (`5` default)
- `Config::featureDim` controls memory/latency tradeoff (`100` default)

Target reference (ESP32 P4): two-frame recognition under 2.0s for small-medium DBs.
Measured results vary by camera pipeline and storage backend.

## Troubleshooting

- SD storage selected but unavailable: library falls back to SPIFFS and logs error
- Ensure face ROI quality before enrollment; blurry ROI is rejected via Laplacian variance check
- For ESP-IDF builds, compile with `USE_ESPIDF`; for Arduino builds, compile with `USE_ARDUINO`
- Run environment checks with:

```bash
bash scripts/check_env.sh
```

## Architecture Diagram

- SVG: `docs/architecture.svg`
- PNG: `docs/architecture.png`

## Versioning

- SemVer, starting at `v1.0.0`
- See `CHANGELOG.md` and `RELEASE.md`