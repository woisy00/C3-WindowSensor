import os
import shutil
from datetime import datetime
from pathlib import Path

Import("env")

# Force clean build to ensure version macros are updated
build_dir = Path(".pio/build/esp32-c3-devkitm-1/src")
print("Trying to clean build directory to update version macros...")
print(f"Build directory path: {build_dir}")
if build_dir.exists():
    shutil.rmtree(build_dir)
    print("[Clean] Removed build directory to update version macros")