#!/usr/bin/env python3
import os
import shutil
import sys

def check_cmd(cmd, hint):
    if shutil.which(cmd):
        print(f"[OK] Found {cmd}")
        return True
    else:
        print(f"[MISSING] {cmd} - {hint}")
        return False

def main():
    ok = True
    print("Checking FaceRecognizeESP build environment...")
    
    if sys.executable:
        print(f"[OK] Found python ({sys.executable})")
    else:
        if not check_cmd("python", "Install Python 3.9+ and add it to PATH") and not check_cmd("python3", "Install Python 3.9+ and add it to PATH"):
            ok = False

    if not check_cmd("cmake", "Install CMake 3.20+"): ok = False
    if not check_cmd("arduino-cli", "Install Arduino CLI for Arduino builds"): ok = False
    if not check_cmd("idf.py", "Install ESP-IDF v5.1+ and export IDF tools"): ok = False

    print("\nChecking filesystem targets...")
    for d in ["spiffs", "sdcard"]:
        try:
            os.makedirs(d, exist_ok=True)
            if os.access(d, os.W_OK):
                print(f"[OK] {d.upper()} host folder is writable: ./{d}")
            else:
                print(f"[MISSING] Cannot write to ./{d}")
                ok = False
        except Exception as e:
            print(f"[MISSING] Cannot create ./{d} : {e}")
            ok = False

    if ok:
        print("\nEnvironment looks ready.")
        sys.exit(0)
    else:
        print("\nOne or more requirements are missing.")
        sys.exit(1)

if __name__ == "__main__":
    main()
