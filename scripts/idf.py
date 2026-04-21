#!/usr/bin/env python3
"""Drop-in proxy for idf.py that auto-configures the ESP-IDF environment.

Usage:
    python scripts/idf.py build
    python scripts/idf.py flash monitor
    python scripts/idf.py menuconfig
    python scripts/idf.py <any idf.py args...>

Automatically finds the ESP-IDF installation and sources export.sh so you
never need to manually set up the environment.
"""

import os
import sys
from pathlib import Path

# Allow importing common.py from the same directory
sys.path.insert(0, str(Path(__file__).resolve().parent))

from common import get_idf_env, _get_idf_python


def main() -> int:
    env = get_idf_env()
    python = _get_idf_python(env)
    idf_py = str(Path(env["IDF_PATH"]) / "tools" / "idf.py")

    cmd = [python, idf_py] + sys.argv[1:]

    os.execve(python, cmd, env)


if __name__ == "__main__":
    sys.exit(main())
