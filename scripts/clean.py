#!/usr/bin/env python3
"""Clean build artifacts."""

import argparse
import shutil
import sys

from common import (
    add_common_args,
    get_build_dir,
    DEBUG_BUILD_DIR,
    RELEASE_BUILD_DIR,
    FIRMWARE_DIR,
    ENV_CACHE_FILE,
)


def _remove(path, label=None):
    """Remove a file or directory, printing what we do."""
    label = label or str(path)
    if path.is_dir():
        print(f"[safemode] Removing {label}")
        shutil.rmtree(path)
    elif path.is_file():
        print(f"[safemode] Removing {label}")
        path.unlink()


def main() -> int:
    parser = argparse.ArgumentParser(description="Clean build artifacts")
    add_common_args(parser)
    parser.add_argument(
        "--all", action="store_true",
        help="Remove everything: all build dirs, generated files, caches"
    )
    args = parser.parse_args()

    if args.all:
        dirs = [DEBUG_BUILD_DIR, RELEASE_BUILD_DIR]
    else:
        dirs = [get_build_dir(args.release)]

    # Build directories
    for build_dir in dirs:
        _remove(build_dir)

    # Generated sdkconfig (regenerated from sdkconfig.defaults on next build)
    _remove(FIRMWARE_DIR / "sdkconfig")

    # Generated web assets
    _remove(FIRMWARE_DIR / "web_assets.h")
    _remove(FIRMWARE_DIR / "web_assets.cmake")

    # Caches
    _remove(FIRMWARE_DIR / ".cache")
    _remove(ENV_CACHE_FILE)

    # Frontend build artifacts
    frontend_dir = FIRMWARE_DIR / "frontend"
    _remove(frontend_dir / "dist")
    _remove(frontend_dir / "node_modules" / ".vite")
    _remove(frontend_dir / "tsconfig.tsbuildinfo")

    # Python caches
    for pycache in (FIRMWARE_DIR.parent / "scripts").rglob("__pycache__"):
        _remove(pycache)

    print("[safemode] Clean complete")
    return 0


if __name__ == "__main__":
    sys.exit(main())
