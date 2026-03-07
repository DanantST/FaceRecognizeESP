# Changelog

## [1.0.0] - 2026-03-07

- Initial release of FaceRecognizeESP.
- Added LBP + PCA feature extraction pipeline.
- Added binary DB format with atomic write and backup recovery.
- Added two-frame recognition engine with top-K centroid prefilter.
- Added SPIFFS, SD, and fake storage adapters.
- Added delayed SD logger (after 2 seconds of engine runtime) with rotation.
- Added unit and integration tests.
- Added Arduino and ESP-IDF example apps.
- Added PCA tooling and CI workflow.