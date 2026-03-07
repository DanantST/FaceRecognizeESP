# Release Notes

## FaceRecognizeESP v1.0.0

### Highlights

- Production-oriented face recognition library for ESP32 class devices.
- Enrolls users using 5 fixed-pose templates.
- Uses LBP histogram + PCA projection and cosine-based matching.
- Persists data in `face_db.bin` with atomic update strategy.
- Supports SD and SPIFFS storage backends.

### Compatibility

- ESP-IDF v5.1+
- Arduino-ESP32 core 3.0.0+

### Included Artifacts

- Library source (`src/`, `include/`, `adapters/`)
- Examples (`examples/arduino`, `examples/espidf`)
- Tests (`tests/unit`, `tests/integration`)
- Tools (`tools/compute_pca.py`, `tools/test_pca.py`)

### Upgrade Notes

- Default `featureDim=100`, `faceSize=96`, `templatesPerUser=5`.
- Update thresholds with `setMatchThreshold()` for your dataset.