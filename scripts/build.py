#!/usr/bin/env python3
"""Build the safemode firmware (and frontend if present)."""

import argparse
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import (
    add_common_args,
    run_idf,
    PROJECT_DIR,
    REPO_ROOT,
)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build safemode firmware")
    add_common_args(parser)
    parser.add_argument(
        "--skip-frontend", action="store_true",
        help="Skip frontend build"
    )
    args = parser.parse_args()

    # Build frontend first (if it exists and not skipped)
    frontend_dir = PROJECT_DIR / "frontend"
    if not args.skip_frontend and frontend_dir.exists():
        print("[safemode] Building frontend...")
        result = subprocess.run(
            [sys.executable, str(REPO_ROOT / "scripts" / "build_frontend.py")],
            cwd=str(PROJECT_DIR),
        )
        if result.returncode != 0:
            print("[safemode] ERROR: Frontend build failed", file=sys.stderr)
            return result.returncode

    # Build firmware
    ret = run_idf("build", release=args.release, verbose=args.verbose)
    if ret != 0:
        return ret

    return 0


if __name__ == "__main__":
    sys.exit(main())
