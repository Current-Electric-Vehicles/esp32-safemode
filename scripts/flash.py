#!/usr/bin/env python3
"""Flash the safemode firmware to the device."""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import (
    add_common_args,
    add_port_args,
    resolve_port,
    run_idf,
)


def main() -> int:
    parser = argparse.ArgumentParser(description="Flash safemode firmware")
    add_common_args(parser)
    add_port_args(parser)
    parser.add_argument(
        "--monitor", action="store_true",
        help="Open serial monitor after flashing"
    )
    parser.add_argument(
        "--app-only", action="store_true",
        help="Flash only the app firmware (fastest for dev iteration)"
    )
    args = parser.parse_args()

    port_args = resolve_port(args)

    idf_args = port_args[:]
    if args.baud:
        idf_args += ["-b", str(args.baud)]
    if args.app_only:
        idf_args.append("app-flash")
    else:
        idf_args.append("flash")
    ret = run_idf(*idf_args, release=args.release, verbose=args.verbose)
    if ret != 0:
        return ret

    if args.monitor:
        idf_args = port_args[:]
        idf_args.append("monitor")
        return run_idf(*idf_args, release=args.release, verbose=args.verbose)

    return 0


if __name__ == "__main__":
    sys.exit(main())
