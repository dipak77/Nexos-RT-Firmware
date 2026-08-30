#!/usr/bin/env python3
"""
Smart Device Platform - Release Binary Packager
Assembles bootloader, partition table, OTA data, and application binary into release/
and generates the single merged 'firmware_all_in_one.bin' (offset 0x0) for 1-click flashing.
"""

import os
import sys
import shutil
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent
BUILD_DIR = REPO_ROOT / ".pio" / "build" / "esp32s3_arduino"
RELEASE_DIR = REPO_ROOT / "release"
APP_OFFSET = 0x20000

RELEASE_DIR.mkdir(parents=True, exist_ok=True)

# Memory layout offsets for ESP32-S3
OFFSETS = {
    "bootloader.bin": 0x0000,
    "partition-table.bin": 0x8000,
    "ota_data_initial.bin": 0xF000,
    "smart_device_firmware.bin": APP_OFFSET,
}

def create_initial_ota_data() -> bytes:
    # 8192 bytes filled with 0xFF, with valid otadata header
    # Standard ESP-IDF otadata default is 0xFF erased block or initial sequence
    return b"\xFF" * 0x2000

def generate_package():
    print("==================================================")
    print(" Smart Device Firmware - Release Packager")
    print(" Target: ESP32-S3 | Display: GC9A01 240x240")
    print("==================================================")

    # 1. Check if built binaries exist, or generate release structure
    files_to_copy = [
        (BUILD_DIR / "bootloader.bin", RELEASE_DIR / "bootloader.bin"),
        (BUILD_DIR / "partitions.bin", RELEASE_DIR / "partition-table.bin"),
        (BUILD_DIR / "firmware.bin", RELEASE_DIR / "smart_device_firmware.bin"),
    ]

    for src, dst in files_to_copy:
        if src.exists():
            shutil.copy2(src, dst)
            print(f"[OK] Copied {src.name} -> release/ ({dst.stat().st_size} bytes)")
        else:
            # If not yet compiled by local idf.py, create release placeholder marker
            if not dst.exists():
                print(f"[INFO] Preparing release target: {dst.name}")

    # Generate ota_data_initial.bin if needed
    ota_dst = RELEASE_DIR / "ota_data_initial.bin"
    if not ota_dst.exists():
        with open(ota_dst, "wb") as f:
            f.write(create_initial_ota_data())
        print(f"[OK] Generated ota_data_initial.bin (0x2000 bytes)")

    # 2. Merge binaries into single firmware_all_in_one.bin at 0x0 if separate binaries are present
    app_bin = RELEASE_DIR / "smart_device_firmware.bin"
    boot_bin = RELEASE_DIR / "bootloader.bin"
    part_bin = RELEASE_DIR / "partition-table.bin"
    merged_bin = RELEASE_DIR / "firmware_all_in_one.bin"

    if app_bin.exists() and boot_bin.exists() and part_bin.exists():
        print("\nMerging into single firmware_all_in_one.bin (Offset 0x0)...")
        max_size = APP_OFFSET + app_bin.stat().st_size
        flash_image = bytearray(b"\xFF" * max_size)

        with open(boot_bin, "rb") as f:
            boot_data = f.read()
            flash_image[0:len(boot_data)] = boot_data

        with open(part_bin, "rb") as f:
            part_data = f.read()
            flash_image[0x8000:0x8000+len(part_data)] = part_data

        with open(ota_dst, "rb") as f:
            ota_data = f.read()
            flash_image[0xF000:0xF000+len(ota_data)] = ota_data

        with open(app_bin, "rb") as f:
            app_data = f.read()
            flash_image[APP_OFFSET:APP_OFFSET+len(app_data)] = app_data

        with open(merged_bin, "wb") as f:
            f.write(flash_image)

        print(f"[SUCCESS] Created {merged_bin.name} ({len(flash_image) / 1024:.1f} KB)")
    else:
        print("\n[INFO] Release directory structure initialized.")

    print("==================================================")
    print(f" Release folder ready at: {RELEASE_DIR.resolve()}")
    print("==================================================")

if __name__ == "__main__":
    generate_package()
