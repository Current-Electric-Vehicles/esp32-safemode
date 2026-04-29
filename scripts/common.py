"""Shared utilities for safemode build scripts."""

import argparse
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
FIRMWARE_DIR = REPO_ROOT / "firmware"
PROJECT_DIR = FIRMWARE_DIR  # ESP-IDF project lives under firmware/
DEFAULT_TARGET = "esp32"
DEBUG_BUILD_DIR = FIRMWARE_DIR / "build"
RELEASE_BUILD_DIR = FIRMWARE_DIR / "build-release"
IDF_PATH_FILE = REPO_ROOT / "scripts" / ".idf_path"
ENV_CACHE_FILE = REPO_ROOT / "scripts" / ".idf_env_cache.json"

PARTITIONS_CSV = FIRMWARE_DIR / "partitions.csv"

def parse_partitions_csv(csv_path: Path = PARTITIONS_CSV) -> dict[str, dict]:
    """Parse partitions.csv and return a dict keyed by partition name.

    Each value is a dict with keys: name, type, subtype, offset, size, flags.
    """
    partitions = {}
    for line in csv_path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 5:
            continue
        name = parts[0]
        partitions[name] = {
            "name": name,
            "type": parts[1],
            "subtype": parts[2],
            "offset": parts[3],
            "size": parts[4],
            "flags": parts[5] if len(parts) > 5 else "",
        }
    return partitions


def get_partition_address(name: str) -> str:
    """Get a flash address from partitions.csv.

    Looks up both data rows (nvs, app, safemode, etc.) and comment-style
    metadata rows like '# bootloader = 0x1000' and '# partition table = 0x10000'.
    """
    # Check data rows first
    partitions = parse_partitions_csv()
    if name in partitions:
        return partitions[name]["offset"]

    # Check comment-style metadata (e.g. "# bootloader = 0x1000")
    lookup = name.replace("-", " ").lower()
    for line in PARTITIONS_CSV.read_text().splitlines():
        line = line.strip()
        if line.startswith("#") and "=" in line:
            key, _, val = line.lstrip("# ").partition("=")
            if key.strip().lower() == lookup:
                return val.strip()

    print(f"[safemode] ERROR: '{name}' not found in {PARTITIONS_CSV}", file=sys.stderr)
    sys.exit(1)


# Common ESP-IDF install locations (checked in order)
_COMMON_IDF_PATHS = [
    Path.home() / "esp" / "esp-idf",
    Path.home() / "esp" / "v6.0" / "esp-idf",
    Path.home() / ".espressif" / "esp-idf",
    Path("/opt/esp-idf"),
]


def _find_idf_path() -> Path:
    """Locate the ESP-IDF installation directory.

    Search order:
    1. IDF_PATH environment variable
    2. scripts/.idf_path file (persisted from previous run or manual config)
    3. Common install locations
    """
    # 1. Environment variable
    env_idf = os.environ.get("IDF_PATH")
    if env_idf:
        p = Path(env_idf)
        if (p / "tools" / "idf.py").exists():
            return p

    # 2. Persisted path file
    if IDF_PATH_FILE.exists():
        p = Path(IDF_PATH_FILE.read_text().strip())
        if (p / "tools" / "idf.py").exists():
            return p
        # Stale config — remove it
        IDF_PATH_FILE.unlink()

    # 3. Common locations
    for p in _COMMON_IDF_PATHS:
        if (p / "tools" / "idf.py").exists():
            return p

    print("[safemode] ERROR: Could not find ESP-IDF installation.", file=sys.stderr)
    print("[safemode]", file=sys.stderr)
    print("[safemode] macOS/Linux — install ESP-IDF v6.0:", file=sys.stderr)
    print("[safemode]   mkdir -p ~/esp && cd ~/esp", file=sys.stderr)
    print("[safemode]   git clone -b v6.0 --recursive https://github.com/espressif/esp-idf.git", file=sys.stderr)
    print("[safemode]   cd esp-idf && ./install.sh esp32", file=sys.stderr)
    print("[safemode]", file=sys.stderr)
    print("[safemode] Windows — install ESP-IDF v6.0:", file=sys.stderr)
    print("[safemode]   Download the ESP-IDF Tools Installer from:", file=sys.stderr)
    print("[safemode]   https://dl.espressif.com/dl/esp-idf/", file=sys.stderr)
    print("[safemode]   Select ESP-IDF v6.0 and ESP32 target during setup.", file=sys.stderr)
    print("[safemode]", file=sys.stderr)
    print("[safemode] Or if ESP-IDF is already installed elsewhere:", file=sys.stderr)
    print("[safemode]   export IDF_PATH=/path/to/esp-idf          # macOS/Linux", file=sys.stderr)
    print("[safemode]   set IDF_PATH=C:\\path\\to\\esp-idf            # Windows", file=sys.stderr)
    print(f"[safemode]   echo /path/to/esp-idf > {IDF_PATH_FILE}", file=sys.stderr)
    sys.exit(1)


def _get_idf_env(idf_path: Path) -> dict[str, str]:
    """Source export.sh and capture the resulting environment.

    Results are cached in .idf_env_cache.json keyed by the IDF_PATH so
    subsequent runs are instant.
    """
    idf_path_str = str(idf_path)
    cache_key = hashlib.sha256(idf_path_str.encode()).hexdigest()[:16]

    # Check cache
    if ENV_CACHE_FILE.exists():
        try:
            cache = json.loads(ENV_CACHE_FILE.read_text())
            if cache.get("key") == cache_key:
                return cache["env"]
        except (json.JSONDecodeError, KeyError):
            pass

    print(f"[safemode] Sourcing ESP-IDF environment from {idf_path} ...")

    # Source export.sh and dump the environment as JSON.
    # Use 'python3' (not sys.executable) so bash resolves it from the
    # post-export PATH, which includes the IDF venv's Python.
    script = (
        f'source "{idf_path}/export.sh" > /dev/null 2>&1 && '
        f'python3 -c "import os,json; print(json.dumps(dict(os.environ)))"'
    )
    result = subprocess.run(
        ["bash", "-c", script],
        capture_output=True, text=True,
        env={**os.environ, "IDF_PATH": idf_path_str},
    )

    if result.returncode != 0:
        print("[safemode] ERROR: Failed to source ESP-IDF export.sh", file=sys.stderr)
        if result.stderr:
            print(result.stderr, file=sys.stderr)
        sys.exit(1)

    try:
        env = json.loads(result.stdout)
    except json.JSONDecodeError:
        print("[safemode] ERROR: Failed to parse environment from export.sh", file=sys.stderr)
        sys.exit(1)

    # Cache for next time
    ENV_CACHE_FILE.write_text(json.dumps({"key": cache_key, "env": env}, indent=2))
    print("[safemode] Environment cached.")

    return env


def get_idf_env() -> dict[str, str]:
    """Get the full ESP-IDF build environment. Auto-detects and caches."""
    idf_path = _find_idf_path()

    # Persist the found path for next time
    if not IDF_PATH_FILE.exists() or IDF_PATH_FILE.read_text().strip() != str(idf_path):
        IDF_PATH_FILE.write_text(str(idf_path))

    return _get_idf_env(idf_path)


def get_build_dir(release: bool) -> Path:
    return RELEASE_BUILD_DIR if release else DEBUG_BUILD_DIR


def get_sdkconfig_defaults(release: bool) -> str:
    """Return the SDKCONFIG_DEFAULTS value for the given build mode."""
    defaults = str(FIRMWARE_DIR / "sdkconfig.defaults")
    if release:
        release_defaults = FIRMWARE_DIR / "sdkconfig.defaults.release"
        if release_defaults.exists():
            defaults += f";{release_defaults}"
    return defaults


def add_common_args(parser: argparse.ArgumentParser) -> None:
    """Add common arguments shared across scripts."""
    parser.add_argument(
        "--release", action="store_true",
        help="Use release build configuration"
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true",
        help="Enable verbose output"
    )


def add_port_args(parser: argparse.ArgumentParser) -> None:
    """Add serial port arguments."""
    parser.add_argument(
        "-p", "--port", type=str, default=None,
        help="Serial port (auto-detected if not specified)"
    )
    parser.add_argument(
        "-b", "--baud", type=int, default=None,
        help="Serial baud rate"
    )


# Known USB VIDs for ESP32 dev boards and common USB-serial chips
_KNOWN_VIDS = {
    0x303A,  # Espressif (built-in USB-JTAG/CDC on ESP32-S2/S3/C3)
    0x10C4,  # Silicon Labs CP210x
    0x0403,  # FTDI
    0x1A86,  # WCH CH340/CH341
    0x2341,  # Arduino
    0x239A,  # Adafruit
}

def detect_serial_port() -> str | None:
    """Auto-detect an ESP32 serial port.

    First checks for known USB VIDs (Espressif, CP210x, FTDI, CH340, etc.),
    then falls back to looking for "usbmodem" or "usbserial" in device names.
    """
    try:
        from serial.tools.list_ports import comports
    except ImportError:
        print("[safemode] WARNING: pyserial not installed, cannot auto-detect port", file=sys.stderr)
        return None

    candidates = [p for p in comports() if p.vid in _KNOWN_VIDS]

    if not candidates:
        candidates = [p for p in comports()
                      if p.device and ("usbmodem" in p.device or "usbserial" in p.device)]

    if not candidates:
        return None

    if len(candidates) == 1:
        port = candidates[0]
        print(f"[safemode] Auto-detected port: {port.device} ({port.product})")
        return port.device

    print("[safemode] Multiple serial devices found:")
    for i, p in enumerate(candidates):
        vid = f"{p.vid:#06x}" if p.vid else "????"
        pid = f"{p.pid:#06x}" if p.pid else "????"
        print(f"  [{i}] {p.device} -- {p.product} ({vid}:{pid})")
    print(f"[safemode] Using: {candidates[0].device} (use -p to override)")
    return candidates[0].device


def resolve_port(args) -> list[str]:
    """Resolve the serial port from args or auto-detection. Returns idf.py flags."""
    port = args.port
    if not port:
        port = detect_serial_port()
    if not port:
        print("[safemode] WARNING: No ESP32 device detected. Letting idf.py try default.", file=sys.stderr)
        return []
    return ["-p", port]


def _get_idf_python(env: dict[str, str]) -> str:
    """Get the Python interpreter from the ESP-IDF environment.

    ESP-IDF sets IDF_PYTHON_ENV_PATH to its virtual environment.
    We prefer that over PATH lookups to avoid pyenv/system Python conflicts.
    """
    # 1. Check IDF_PYTHON_ENV_PATH (most reliable — set by ESP-IDF export.sh)
    idf_venv = env.get("IDF_PYTHON_ENV_PATH", "")
    if idf_venv:
        for name in ("python3", "python"):
            candidate = Path(idf_venv) / "bin" / name
            if candidate.is_file():
                return str(candidate)

    # 2. Search PATH from the captured env, skipping known non-IDF Python locations
    skip_patterns = (".pyenv", ".platformio", "/usr/bin", "/usr/local/bin")
    path_dirs = env.get("PATH", "").split(os.pathsep)
    for d in path_dirs:
        if any(pat in d for pat in skip_patterns):
            continue
        for name in ("python3", "python"):
            candidate = Path(d) / name
            if candidate.is_file():
                return str(candidate)

    # 3. Fallback — search PATH without skip filter
    for d in path_dirs:
        for name in ("python3", "python"):
            candidate = Path(d) / name
            if candidate.is_file():
                return str(candidate)

    # 4. Last resort
    return sys.executable


def _get_idf_version(idf_path: Path) -> tuple[str, str, str]:
    """Read IDF version from tools/cmake/version.cmake.

    Parses lines like:
        set(IDF_VERSION_MAJOR 6)
        set(IDF_VERSION_MINOR 0)
        set(IDF_VERSION_PATCH 0)
    """
    import re
    version_cmake = idf_path / "tools" / "cmake" / "version.cmake"
    major, minor, patch = "6", "0", "0"
    if version_cmake.exists():
        text = version_cmake.read_text()
        m = re.search(r'set\(IDF_VERSION_MAJOR\s+(\d+)\)', text)
        if m: major = m.group(1)
        m = re.search(r'set\(IDF_VERSION_MINOR\s+(\d+)\)', text)
        if m: minor = m.group(1)
        m = re.search(r'set\(IDF_VERSION_PATCH\s+(\d+)\)', text)
        if m: patch = m.group(1)
    return major, minor, patch


def _find_idf_python(idf_path: Path) -> tuple[str, str]:
    """Find the ESP-IDF Python venv interpreter by matching IDF version.

    Returns (python_path, venv_path).

    ESP-IDF installs its Python venv at:
      ~/.espressif/python_env/idf{MAJOR}.{MINOR}_py{PYVER}_env/bin/python3
    """
    major, minor, _ = _get_idf_version(idf_path)

    # Search for matching venv
    espressif_dir = Path.home() / ".espressif" / "python_env"
    if espressif_dir.exists():
        prefix = f"idf{major}.{minor}_py"
        for d in sorted(espressif_dir.iterdir(), reverse=True):
            if d.name.startswith(prefix):
                python = d / "bin" / "python3"
                if python.is_file():
                    return str(python), str(d)

    # Fallback: try any idf venv
    if espressif_dir.exists():
        for d in sorted(espressif_dir.iterdir(), reverse=True):
            if d.name.startswith("idf"):
                python = d / "bin" / "python3"
                if python.is_file():
                    return str(python), str(d)

    print("[safemode] ERROR: Could not find ESP-IDF Python venv.", file=sys.stderr)
    print(f"[safemode] Expected at: {espressif_dir}/idf{major}.{minor}_py*_env/bin/python3", file=sys.stderr)
    print("[safemode] Try running: cd ~/esp/esp-idf && ./install.sh esp32", file=sys.stderr)
    sys.exit(1)


def run_idf(*args: str, release: bool = False, verbose: bool = False,
            extra_env: dict | None = None) -> int:
    """Run idf.py by sourcing export.sh to get the correct tool versions.

    We use the IDF venv Python to capture the post-export environment,
    then run idf.py with that environment. This ensures the correct
    toolchain versions are on PATH (not just any installed version).
    """
    idf_path = _find_idf_path()
    python, venv_path = _find_idf_python(idf_path)
    build_dir = get_build_dir(release)

    idf_args = ["-B", str(build_dir)]
    if verbose:
        idf_args += ["-v"]
    idf_args += list(args)
    args_str = " ".join(idf_args)

    # Use the IDF venv Python to capture the export.sh environment,
    # then run idf.py in that environment
    bash_cmd = (
        f'export IDF_PATH="{idf_path}" && '
        f'source "{idf_path}/export.sh" > /dev/null 2>&1 && '
        f'SDKCONFIG_DEFAULTS="{get_sdkconfig_defaults(release)}" '
        f'python3 "{idf_path}/tools/idf.py" {args_str}'
    )

    env = {**os.environ, "IDF_PATH": str(idf_path)}
    if extra_env:
        env.update(extra_env)
    # Ensure the IDF venv Python is first on PATH for the bash subprocess
    env["PATH"] = str(Path(venv_path) / "bin") + os.pathsep + env.get("PATH", "")

    mode = "release" if release else "debug"
    print(f"[safemode] Mode: {mode}")
    print(f"[safemode] Python: {python}")
    print(f"[safemode] Build dir: {build_dir}")
    print(f"[safemode] Running: idf.py {args_str}")
    print()

    result = subprocess.run(["bash", "-c", bash_cmd], cwd=str(FIRMWARE_DIR), env=env)
    return result.returncode


# ---- esptool helpers ----

def _find_esptool(name: str) -> str:
    """Find an esptool command (espsecure, esptool, espefuse)."""
    base_name = name.removesuffix(".py")
    candidates = [base_name, name] if base_name != name else [name]

    idf_path = _find_idf_path()
    _, venv_path = _find_idf_python(idf_path)
    for n in candidates:
        candidate = Path(venv_path) / "bin" / n
        if candidate.is_file():
            return str(candidate)

    import shutil as _shutil
    for n in candidates:
        found = _shutil.which(n)
        if found:
            return found

    print(f"[safemode] ERROR: Could not find {base_name}", file=sys.stderr)
    sys.exit(1)


def get_idf_python() -> str:
    """Return the IDF venv Python path."""
    idf_path = _find_idf_path()
    python, _ = _find_idf_python(idf_path)
    return python


def run_esptool(*args: str) -> int:
    """Run esptool with the given arguments."""
    python = get_idf_python()
    tool = _find_esptool("esptool")
    cmd = [python, tool] + list(args)
    print(f"[safemode] {' '.join(cmd)}")
    return subprocess.run(cmd, cwd=str(FIRMWARE_DIR)).returncode
