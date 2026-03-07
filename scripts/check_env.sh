#!/usr/bin/env bash
set -euo pipefail

ok=true

check_cmd() {
  local cmd="$1"
  local hint="$2"
  if command -v "$cmd" >/dev/null 2>&1; then
    echo "[OK] Found $cmd"
  else
    echo "[MISSING] $cmd - $hint"
    ok=false
  fi
}

echo "Checking FaceRecognizeESP build environment..."
check_cmd python3 "Install Python 3.9+ and add it to PATH"
check_cmd cmake "Install CMake 3.20+"
check_cmd arduino-cli "Install Arduino CLI for Arduino builds"
check_cmd idf.py "Install ESP-IDF v5.1+ and export IDF tools"

echo "Checking filesystem targets..."
if [ -d ./spiffs ] || mkdir -p ./spiffs; then
  echo "[OK] SPIFFS/LittleFS host folder is writable: ./spiffs"
else
  echo "[MISSING] Cannot create ./spiffs"
  ok=false
fi

if [ -d ./sdcard ] || mkdir -p ./sdcard; then
  echo "[OK] SD host folder is writable: ./sdcard"
else
  echo "[MISSING] Cannot create ./sdcard"
  ok=false
fi

if [ "$ok" = true ]; then
  echo "Environment looks ready."
  exit 0
fi

echo "One or more requirements are missing."
exit 1