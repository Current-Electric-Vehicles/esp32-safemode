#!/usr/bin/env python3
"""Open the ESP-IDF menuconfig."""

import argparse
import sys

from common import add_common_args, run_idf


def main() -> int:
    parser = argparse.ArgumentParser(description="Open ESP-IDF menuconfig")
    add_common_args(parser)
    args = parser.parse_args()

    return run_idf("menuconfig", release=args.release, verbose=args.verbose)


if __name__ == "__main__":
    sys.exit(main())
