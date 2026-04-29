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
    run_esptool,
    get_build_dir,
    get_partition_address,
)

PROJECT_NAME = "safemode"


def _esptool_write_flash(port_args: list[str], baud: int | None,
                         addr_file_pairs: list[tuple[str, Path]]) -> int:
    """Write one or more binaries to flash via esptool."""
    esptool_args = port_args + [
        "-b", str(baud or 921600),
        "write-flash",
        "--flash-mode", "dio",
        "--flash-size", "detect",
        "--flash-freq", "40m",
    ]
    for addr, path in addr_file_pairs:
        esptool_args += [addr, str(path)]
    return run_esptool(*esptool_args)


def main() -> int:
    parser = argparse.ArgumentParser(description="Flash safemode firmware")
    add_common_args(parser)
    add_port_args(parser)
    parser.add_argument(
        "--monitor", action="store_true",
        help="Open serial monitor after flashing"
    )
    parser.add_argument(
        "--safemode-only", action="store_true",
        help="Flash only the safemode partition"
    )
    parser.add_argument(
        "--app-only", action="store_true",
        help="Flash only the app firmware (fastest for dev iteration)"
    )
    args = parser.parse_args()

    build_dir = get_build_dir(args.release)
    port_args = resolve_port(args)

    addr_bootloader = get_partition_address("bootloader")
    addr_partition_table = get_partition_address("partition table")
    addr_safemode = get_partition_address("safemode")

    bootloader_bin = build_dir / "bootloader" / "bootloader.bin"
    partition_table_bin = build_dir / "partition_table" / "partition-table.bin"
    safemode_bin = build_dir / f"{PROJECT_NAME}.bin"

    if args.safemode_only:
        if not safemode_bin.exists():
            print(f"[safemode] ERROR: {safemode_bin} not found. Run build.py first.", file=sys.stderr)
            return 1
        print(f"[safemode] Flashing safemode @ {addr_safemode}")
        ret = _esptool_write_flash(port_args, args.baud, [
            (addr_safemode, safemode_bin),
        ])
        if ret != 0:
            return ret

    elif args.app_only:
        # "app-only" in safemode context means just the safemode binary
        # (this is the app we're building). Same as --safemode-only.
        if not safemode_bin.exists():
            print(f"[safemode] ERROR: {safemode_bin} not found. Run build.py first.", file=sys.stderr)
            return 1
        print(f"[safemode] Flashing safemode (app-only) @ {addr_safemode}")
        ret = _esptool_write_flash(port_args, args.baud, [
            (addr_safemode, safemode_bin),
        ])
        if ret != 0:
            return ret

    else:
        missing = []
        for desc, path in [("bootloader", bootloader_bin),
                           ("partition table", partition_table_bin),
                           ("safemode firmware", safemode_bin)]:
            if not path.exists():
                missing.append(f"  {desc}: {path}")
        if missing:
            print("[safemode] ERROR: Missing binaries. Run build.py first:", file=sys.stderr)
            for m in missing:
                print(m, file=sys.stderr)
            return 1

        pairs: list[tuple[str, Path]] = [
            (addr_bootloader, bootloader_bin),
            (addr_partition_table, partition_table_bin),
            (addr_safemode, safemode_bin),
        ]

        print("[safemode] Flashing all binaries:")
        for addr, path in pairs:
            print(f"  {path.name} @ {addr}")

        ret = _esptool_write_flash(port_args, args.baud, pairs)
        if ret != 0:
            return ret

    if args.monitor:
        import time
        time.sleep(1)
        if not port_args:
            print("\n[safemode] Waiting for device to reboot...")
            time.sleep(2)
            port_args = resolve_port(args)
        monitor_args = port_args[:]
        monitor_args.append("monitor")
        return run_idf(*monitor_args, release=args.release, verbose=args.verbose)

    return 0


if __name__ == "__main__":
    sys.exit(main())
