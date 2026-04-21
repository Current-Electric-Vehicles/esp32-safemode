#!/usr/bin/env python3
"""Clean build artifacts."""

import argparse
import shutil
import sys

from common import add_common_args, get_build_dir


def main() -> int:
    parser = argparse.ArgumentParser(description="Clean build artifacts")
    add_common_args(parser)
    parser.add_argument(
        "--all", action="store_true",
        help="Remove both debug and release build directories"
    )
    args = parser.parse_args()

    if args.all:
        from common import DEBUG_BUILD_DIR, RELEASE_BUILD_DIR
        dirs = [DEBUG_BUILD_DIR, RELEASE_BUILD_DIR]
    else:
        dirs = [get_build_dir(args.release)]

    for build_dir in dirs:
        if build_dir.exists():
            print(f"[safemode] Removing {build_dir}")
            shutil.rmtree(build_dir)
        else:
            print(f"[safemode] {build_dir} does not exist, nothing to clean")

    return 0


if __name__ == "__main__":
    sys.exit(main())
