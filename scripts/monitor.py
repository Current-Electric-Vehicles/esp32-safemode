#!/usr/bin/env python3
"""Open the serial monitor."""

import argparse
import sys

from common import add_common_args, add_port_args, resolve_port, run_idf


def main() -> int:
    parser = argparse.ArgumentParser(description="Open serial monitor")
    add_common_args(parser)
    add_port_args(parser)
    args = parser.parse_args()

    idf_args = resolve_port(args)

    if args.baud:
        idf_args += ["-b", str(args.baud)]

    idf_args.append("monitor")

    return run_idf(*idf_args, release=args.release, verbose=args.verbose)


if __name__ == "__main__":
    sys.exit(main())
