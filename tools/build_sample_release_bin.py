#!/usr/bin/env python3
"""Compatibility entry point for the real firmware release packager.

This script previously emitted synthetic ESP32 images containing placeholder
bytes. Those files looked flashable but were not a runnable application. Keep
the old command name as a safe alias that packages only compiled artifacts.
"""

from generate_release_binaries import generate_package


if __name__ == "__main__":
    generate_package()
