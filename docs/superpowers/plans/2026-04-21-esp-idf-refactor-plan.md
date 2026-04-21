# ESP-IDF Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite the safemode firmware from PlatformIO/Arduino to pure ESP-IDF 6.0 with component architecture, Python build scripts, and a React 19/Tailwind v4 frontend.

**Architecture:** Two ESP-IDF components (`ota` and `wifi`) wired together in `main/main.cpp`. Python scripts in `scripts/` handle build orchestration, frontend compilation, and flashing. Frontend is a single-page React app embedded into the `wifi` component as gzipped binary assets via ESP-IDF's `EMBED_FILES`.

**Tech Stack:** ESP-IDF 6.0, C++20, React 19, Tailwind CSS v4, Vite 8, TypeScript 6, Python 3

**Spec:** `docs/superpowers/specs/2026-04-21-esp-idf-refactor-design.md`

**Reference projects:**
- `~/Projects/current-hcu` — UI framework, build scripts, component patterns
- `~/Projects/current-ledcontroller` — build scripts, refactor loop plan

---

### Task 1: Delete old PlatformIO/Arduino code and scaffold ESP-IDF project

**Files:**
- Delete: `firmware/platformio.ini`, `firmware/platformio.local.ini`, `firmware/build_web.py`, `firmware/dependencies.lock`
- Delete: `firmware/src/` (entire directory)
- Delete: `firmware/include/` (entire directory — StaticFiles.h is generated, SafemodeWebServer.h is replaced)
- Delete: `firmware/sdkconfig`, `firmware/sdkconfig.local`, `firmware/sdkconfig.local.old`
- Delete: `firmware/.pio/` (build artifacts)
- Delete: `firmware/.vscode/` (PlatformIO IDE config)
- Delete: `firmware/.idea/` (IDE config)
- Delete: `firmware/data/` (empty data dir)
- Delete: `firmware/README.md` (placeholder)
- Delete: `web/` (entire directory — will be recreated as `firmware/frontend/`)
- Delete: `current-esp32-safemode.code-workspace` (PlatformIO workspace file)
- Keep: `firmware/partitions.csv` (unchanged)
- Keep: `firmware/.gitignore` (will be updated)
- Overwrite: `firmware/CMakeLists.txt`
- Create: `firmware/main/CMakeLists.txt`
- Create: `firmware/main/main.cpp` (minimal stub)
- Create: `firmware/sdkconfig.defaults`
- Create: `firmware/sdkconfig.defaults.release`

- [ ] **Step 1: Delete old PlatformIO and Arduino files**

```bash
cd /Users/briandilley/Projects/current-esp32-safemode

# Delete PlatformIO config and build artifacts
rm -f firmware/platformio.ini firmware/platformio.local.ini firmware/build_web.py firmware/dependencies.lock
rm -rf firmware/.pio firmware/.vscode firmware/.idea firmware/data

# Delete old source code
rm -rf firmware/src firmware/include

# Delete old sdkconfig files (will be replaced by sdkconfig.defaults)
rm -f firmware/sdkconfig firmware/sdkconfig.local firmware/sdkconfig.local.old
rm -f firmware/README.md

# Delete old web directory (moving to firmware/frontend/)
rm -rf web

# Delete workspace file
rm -f current-esp32-safemode.code-workspace
```

- [ ] **Step 2: Create top-level CMakeLists.txt**

Write `firmware/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(safemode)
```

- [ ] **Step 3: Create main component**

Create directory `firmware/main/` and write `firmware/main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
)
```

Write `firmware/main/main.cpp` (minimal stub that compiles):

```cpp
#include "esp_log.h"
#include "nvs_flash.h"

static constexpr const char* kTag = "safemode";

extern "C" void app_main()
{
    ESP_LOGI(kTag, "Safemode starting...");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(kTag, "NVS partition truncated, erasing...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(kTag, "Safemode initialized (stub)");

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

- [ ] **Step 4: Create sdkconfig.defaults**

Write `firmware/sdkconfig.defaults`:

```
# Safemode Default Configuration

# Target
CONFIG_IDF_TARGET="esp32"

# Flash
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y

# Custom partition table
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"

# Compiler
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
# CONFIG_COMPILER_CXX_EXCEPTIONS is not set
# CONFIG_COMPILER_CXX_RTTI is not set

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y

# FreeRTOS
CONFIG_FREERTOS_HZ=1000

# Watchdog
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10

# Panic handler
CONFIG_ESP_SYSTEM_PANIC_PRINT_HALT=y

# HTTP server
CONFIG_HTTPD_MAX_URI_HANDLERS=16

# LWIP
CONFIG_LWIP_MAX_SOCKETS=8
```

Write `firmware/sdkconfig.defaults.release`:

```
# Release overrides
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
CONFIG_LOG_DEFAULT_LEVEL_WARN=y
```

- [ ] **Step 5: Update firmware/.gitignore**

Write `firmware/.gitignore`:

```
build/
build-release/
sdkconfig
sdkconfig.old
managed_components/
.idea/
```

- [ ] **Step 6: Create root .gitignore**

Write `.gitignore` at repo root:

```
.DS_Store
*.swp
*.swo
*~
```

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "refactor: remove PlatformIO/Arduino, scaffold ESP-IDF project

Delete all PlatformIO config, Arduino source, old web directory, and
sdkconfig files. Create minimal ESP-IDF project structure with main
stub, sdkconfig.defaults, and updated gitignore.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Create Python build scripts

**Files:**
- Create: `scripts/common.py`
- Create: `scripts/build.py`
- Create: `scripts/build_frontend.py`
- Create: `scripts/flash.py`
- Create: `scripts/clean.py`
- Create: `scripts/monitor.py`
- Create: `scripts/menuconfig.py`
- Create: `scripts/idf.py`

- [ ] **Step 1: Create scripts/common.py**

Adapted from `~/Projects/current-hcu/scripts/common.py` with encryption removed, ESP32 addresses, and `[safemode]` prefix.

```python
#!/usr/bin/env python3
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

# Flash layout — ESP32
PARTITIONS_CSV = FIRMWARE_DIR / "partitions.csv"
ADDR_BOOTLOADER = "0x1000"
ADDR_PARTITION_TABLE = "0x8000"


def parse_partitions_csv(csv_path: Path = PARTITIONS_CSV) -> dict[str, dict]:
    """Parse partitions.csv and return a dict keyed by partition name."""
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
    """Get the flash offset for a named partition from partitions.csv."""
    partitions = parse_partitions_csv()
    if name not in partitions:
        print(f"[safemode] ERROR: Partition '{name}' not found in {PARTITIONS_CSV}", file=sys.stderr)
        sys.exit(1)
    return partitions[name]["offset"]


# Common ESP-IDF install locations (checked in order)
_COMMON_IDF_PATHS = [
    Path.home() / "esp" / "esp-idf",
    Path.home() / "esp" / "v6.0" / "esp-idf",
    Path.home() / ".espressif" / "esp-idf",
    Path("/opt/esp-idf"),
]


def _find_idf_path() -> Path:
    """Locate the ESP-IDF installation directory."""
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
    print("[safemode] Or if ESP-IDF is already installed elsewhere:", file=sys.stderr)
    print("[safemode]   export IDF_PATH=/path/to/esp-idf", file=sys.stderr)
    print(f"[safemode]   echo /path/to/esp-idf > {IDF_PATH_FILE}", file=sys.stderr)
    sys.exit(1)


def _get_idf_env(idf_path: Path) -> dict[str, str]:
    """Source export.sh and capture the resulting environment (cached)."""
    idf_path_str = str(idf_path)
    cache_key = hashlib.sha256(idf_path_str.encode()).hexdigest()[:16]

    if ENV_CACHE_FILE.exists():
        try:
            cache = json.loads(ENV_CACHE_FILE.read_text())
            if cache.get("key") == cache_key:
                return cache["env"]
        except (json.JSONDecodeError, KeyError):
            pass

    print(f"[safemode] Sourcing ESP-IDF environment from {idf_path} ...")

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

    ENV_CACHE_FILE.write_text(json.dumps({"key": cache_key, "env": env}, indent=2))
    print("[safemode] Environment cached.")
    return env


def get_idf_env() -> dict[str, str]:
    """Get the full ESP-IDF build environment. Auto-detects and caches."""
    idf_path = _find_idf_path()
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
    parser.add_argument("--release", action="store_true", help="Use release build configuration")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose output")


def add_port_args(parser: argparse.ArgumentParser) -> None:
    """Add serial port arguments."""
    parser.add_argument("-p", "--port", type=str, default=None, help="Serial port (auto-detected if not specified)")
    parser.add_argument("-b", "--baud", type=int, default=None, help="Serial baud rate")


# Espressif USB VID
_ESPRESSIF_VID = 0x303A


def detect_serial_port() -> str | None:
    """Auto-detect an Espressif ESP32 serial port."""
    try:
        from serial.tools.list_ports import comports
    except ImportError:
        print("[safemode] WARNING: pyserial not installed, cannot auto-detect port", file=sys.stderr)
        return None

    candidates = [p for p in comports() if p.vid == _ESPRESSIF_VID]

    if not candidates:
        return None

    if len(candidates) == 1:
        port = candidates[0]
        print(f"[safemode] Auto-detected port: {port.device} ({port.product})")
        return port.device

    print("[safemode] Multiple Espressif devices found:")
    for i, p in enumerate(candidates):
        print(f"  [{i}] {p.device} — {p.product} ({p.vid:#06x}:{p.pid:#06x})")
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


def _get_idf_version(idf_path: Path) -> tuple[str, str, str]:
    """Read IDF version from tools/cmake/version.cmake."""
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
    """Find the ESP-IDF Python venv interpreter by matching IDF version."""
    major, minor, _ = _get_idf_version(idf_path)

    espressif_dir = Path.home() / ".espressif" / "python_env"
    if espressif_dir.exists():
        prefix = f"idf{major}.{minor}_py"
        for d in sorted(espressif_dir.iterdir(), reverse=True):
            if d.name.startswith(prefix):
                python = d / "bin" / "python3"
                if python.is_file():
                    return str(python), str(d)

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


def _get_idf_python(env: dict[str, str]) -> str:
    """Get the Python interpreter from the ESP-IDF environment."""
    idf_venv = env.get("IDF_PYTHON_ENV_PATH", "")
    if idf_venv:
        for name in ("python3", "python"):
            candidate = Path(idf_venv) / "bin" / name
            if candidate.is_file():
                return str(candidate)

    skip_patterns = (".pyenv", ".platformio", "/usr/bin", "/usr/local/bin")
    path_dirs = env.get("PATH", "").split(os.pathsep)
    for d in path_dirs:
        if any(pat in d for pat in skip_patterns):
            continue
        for name in ("python3", "python"):
            candidate = Path(d) / name
            if candidate.is_file():
                return str(candidate)

    for d in path_dirs:
        for name in ("python3", "python"):
            candidate = Path(d) / name
            if candidate.is_file():
                return str(candidate)

    return sys.executable


def run_idf(*args: str, release: bool = False, verbose: bool = False,
            extra_env: dict | None = None) -> int:
    """Run idf.py by sourcing export.sh to get the correct tool versions."""
    idf_path = _find_idf_path()
    python, venv_path = _find_idf_python(idf_path)
    build_dir = get_build_dir(release)

    idf_args = ["-B", str(build_dir)]
    if verbose:
        idf_args += ["-v"]
    idf_args += list(args)
    args_str = " ".join(idf_args)

    bash_cmd = (
        f'export IDF_PATH="{idf_path}" && '
        f'source "{idf_path}/export.sh" > /dev/null 2>&1 && '
        f'SDKCONFIG_DEFAULTS="{get_sdkconfig_defaults(release)}" '
        f'python3 "{idf_path}/tools/idf.py" {args_str}'
    )

    env = {**os.environ, "IDF_PATH": str(idf_path)}
    if extra_env:
        env.update(extra_env)
    env["PATH"] = str(Path(venv_path) / "bin") + os.pathsep + env.get("PATH", "")

    mode = "release" if release else "debug"
    print(f"[safemode] Mode: {mode}")
    print(f"[safemode] Python: {python}")
    print(f"[safemode] Build dir: {build_dir}")
    print(f"[safemode] Running: idf.py {args_str}")
    print()

    result = subprocess.run(["bash", "-c", bash_cmd], cwd=str(FIRMWARE_DIR), env=env)
    return result.returncode
```

- [ ] **Step 2: Create scripts/build.py**

```python
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
```

- [ ] **Step 3: Create scripts/build_frontend.py**

```python
#!/usr/bin/env python3
"""Build the frontend and generate embedded file assets for the firmware."""

import gzip
import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import PROJECT_DIR

FRONTEND_DIR = PROJECT_DIR / "frontend"
DIST_DIR = FRONTEND_DIR / "dist"
WEB_DIR = PROJECT_DIR / "components" / "wifi" / "web"
GENERATED_HEADER = PROJECT_DIR / "components" / "wifi" / "src" / "web_assets.h"

# Map file extensions to MIME types
MIME_TYPES = {
    ".html": "text/html",
    ".js": "application/javascript",
    ".css": "text/css",
    ".woff2": "font/woff2",
    ".woff": "font/woff",
    ".ttf": "font/ttf",
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".svg": "image/svg+xml",
    ".ico": "image/x-icon",
    ".json": "application/json",
    ".gif": "image/gif",
    ".webp": "image/webp",
    ".map": "application/json",
}

# Extensions that benefit from gzip compression
GZIP_EXTENSIONS = {".html", ".js", ".css", ".svg", ".json", ".map"}


def find_nvm_node() -> str | None:
    """Find nvm and return the path to use node via nvm."""
    nvm_dir = os.environ.get("NVM_DIR", str(Path.home() / ".nvm"))
    nvm_sh = Path(nvm_dir) / "nvm.sh"
    if nvm_sh.exists():
        return str(nvm_sh)
    return None


def _check_node_available() -> bool:
    """Check if node/npm are available on PATH."""
    try:
        subprocess.run(["node", "--version"], capture_output=True, check=True)
        return True
    except (FileNotFoundError, subprocess.CalledProcessError):
        return False


def build_frontend() -> int:
    """Build the frontend using npm (via nvm if available)."""
    print("[safemode] Building frontend...")

    nvm_sh = find_nvm_node()
    node_modules = FRONTEND_DIR / "node_modules"

    if not nvm_sh and not _check_node_available():
        print("[safemode] ERROR: Node.js is not installed.", file=sys.stderr)
        print("[safemode]", file=sys.stderr)
        print("[safemode] macOS/Linux — install nvm (recommended):", file=sys.stderr)
        print("[safemode]   curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.3/install.sh | bash", file=sys.stderr)
        print("[safemode]   nvm install", file=sys.stderr)
        print("[safemode]", file=sys.stderr)
        print("[safemode] Or install Node.js directly: https://nodejs.org/", file=sys.stderr)
        return 1

    if nvm_sh:
        nvm_prefix = f'source "{nvm_sh}" && nvm use'
        if not node_modules.exists():
            print("[safemode] Installing frontend dependencies...")
            install_result = subprocess.run(
                ["bash", "-c", f'{nvm_prefix} && npm install'],
                cwd=str(FRONTEND_DIR),
            )
            if install_result.returncode != 0:
                print("[safemode] ERROR: npm install failed", file=sys.stderr)
                return install_result.returncode
        result = subprocess.run(
            ["bash", "-c", f'{nvm_prefix} && npm run build'],
            cwd=str(FRONTEND_DIR),
        )
    else:
        if not node_modules.exists():
            print("[safemode] Installing frontend dependencies...")
            install_result = subprocess.run(
                ["npm", "install"],
                cwd=str(FRONTEND_DIR),
            )
            if install_result.returncode != 0:
                print("[safemode] ERROR: npm install failed", file=sys.stderr)
                return install_result.returncode
        result = subprocess.run(
            ["npm", "run", "build"],
            cwd=str(FRONTEND_DIR),
        )

    if result.returncode != 0:
        print("[safemode] ERROR: Frontend build failed", file=sys.stderr)
        return result.returncode

    print("[safemode] Frontend built successfully")
    return 0


def symbol_name(rel_path: str) -> str:
    """Convert a file path to a valid C symbol name."""
    return rel_path.replace("/", "_").replace(".", "_").replace("-", "_")


def collect_files() -> list[dict]:
    """Collect all files from the frontend dist directory."""
    if not DIST_DIR.exists():
        return []

    files = []
    for path in sorted(DIST_DIR.rglob("*")):
        if not path.is_file():
            continue

        rel_path = path.relative_to(DIST_DIR)
        url_path = "/" + str(rel_path).replace("\\", "/")
        embed_path = "web/" + str(rel_path).replace("\\", "/")
        sym = symbol_name(path.name)
        ext = path.suffix.lower()
        mime = MIME_TYPES.get(ext, "application/octet-stream")

        files.append({
            "url_path": url_path,
            "embed_path": embed_path,
            "symbol": sym,
            "mime_type": mime,
            "size": path.stat().st_size,
            "is_gzipped": ext in GZIP_EXTENSIONS,
        })

    return files


def copy_dist_to_web():
    """Copy frontend/dist/ to components/wifi/web/, gzip-compressing text assets."""
    if WEB_DIR.exists():
        shutil.rmtree(WEB_DIR)
    shutil.copytree(DIST_DIR, WEB_DIR)
    for path in WEB_DIR.rglob("*"):
        if path.is_file() and path.suffix.lower() in GZIP_EXTENSIONS:
            original_size = path.stat().st_size
            with open(path, "rb") as f_in:
                data = f_in.read()
            compressed = gzip.compress(data, compresslevel=9)
            with open(path, "wb") as f_out:
                f_out.write(compressed)
            print(f"[safemode]   gzip: {path.name} {original_size:,} -> {len(compressed):,} bytes")
    print(f"[safemode] Copied {DIST_DIR} -> {WEB_DIR}")


def generate_header(files: list[dict]):
    """Generate web_assets.h with embedded file references."""
    lines = [
        "#pragma once",
        "",
        "// Auto-generated by scripts/build_frontend.py — do not edit",
        "",
        '#include <cstddef>',
        '#include <cstdint>',
        "",
    ]

    for f in files:
        sym = f["symbol"]
        lines.append(f'extern const uint8_t {sym}_start[] asm("_binary_{sym}_start");')
        lines.append(f'extern const uint8_t {sym}_end[] asm("_binary_{sym}_end");')
        lines.append("")

    lines.append("struct WebAsset")
    lines.append("{")
    lines.append("    const char* urlPath;")
    lines.append("    const char* mimeType;")
    lines.append("    const uint8_t* data;")
    lines.append("    size_t size;")
    lines.append("    bool isGzipped;")
    lines.append("};")
    lines.append("")

    lines.append(f"static constexpr int kWebAssetCount = {len(files)};")
    lines.append("")
    lines.append("inline const WebAsset kWebAssets[] = {")
    for f in files:
        sym = f["symbol"]
        url = f["url_path"]
        mime = f["mime_type"]
        gz = "true" if f["is_gzipped"] else "false"
        lines.append(f'    {{"{url}", "{mime}", {sym}_start,')
        lines.append(f'     static_cast<size_t>({sym}_end - {sym}_start), {gz}}},')
    lines.append("};")
    lines.append("")

    GENERATED_HEADER.write_text("\n".join(lines))
    print(f"[safemode] Generated {GENERATED_HEADER} ({len(files)} assets)")


def generate_cmake_embed_list(files: list[dict]):
    """Generate a CMake file listing all embedded files."""
    cmake_path = PROJECT_DIR / "components" / "wifi" / "web_assets.cmake"
    embed_paths = [f['embed_path'] for f in files]
    lines = [
        "# Auto-generated by scripts/build_frontend.py — do not edit",
        "set(WEB_EMBED_FILES",
    ]
    for p in embed_paths:
        lines.append(f'    "{p}"')
    lines.append(")")
    cmake_path.write_text("\n".join(lines) + "\n")
    print(f"[safemode] Generated {cmake_path}")


def main() -> int:
    ret = build_frontend()
    if ret != 0:
        return ret

    copy_dist_to_web()
    files = collect_files()

    if not files:
        print("[safemode] WARNING: No frontend files found!", file=sys.stderr)
        return 1

    generate_header(files)
    generate_cmake_embed_list(files)

    total_size = sum(f["size"] for f in files)
    print(f"[safemode] {len(files)} files, {total_size:,} bytes total")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Create scripts/flash.py**

```python
#!/usr/bin/env python3
"""Flash the safemode firmware to the device."""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import add_common_args, add_port_args, resolve_port, run_idf


def main() -> int:
    parser = argparse.ArgumentParser(description="Flash safemode firmware")
    add_common_args(parser)
    add_port_args(parser)
    parser.add_argument("--monitor", action="store_true", help="Open serial monitor after flashing")
    parser.add_argument("--app-only", action="store_true", help="Flash only the app firmware (fastest for dev iteration)")
    args = parser.parse_args()

    idf_args = resolve_port(args)
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
        idf_args_mon = resolve_port(args)
        idf_args_mon.append("monitor")
        return run_idf(*idf_args_mon, release=args.release, verbose=args.verbose)

    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 5: Create scripts/clean.py**

```python
#!/usr/bin/env python3
"""Clean build artifacts."""

import argparse
import shutil
import sys

from common import add_common_args, get_build_dir, DEBUG_BUILD_DIR, RELEASE_BUILD_DIR


def main() -> int:
    parser = argparse.ArgumentParser(description="Clean build artifacts")
    add_common_args(parser)
    parser.add_argument("--all", action="store_true", help="Remove both debug and release build directories")
    args = parser.parse_args()

    if args.all:
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
```

- [ ] **Step 6: Create scripts/monitor.py**

```python
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
```

- [ ] **Step 7: Create scripts/menuconfig.py**

```python
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
```

- [ ] **Step 8: Create scripts/idf.py**

```python
#!/usr/bin/env python3
"""Drop-in proxy for idf.py that auto-configures the ESP-IDF environment.

Usage:
    python scripts/idf.py build
    python scripts/idf.py flash monitor
    python scripts/idf.py menuconfig
    python scripts/idf.py <any idf.py args...>
"""

import os
import sys
from pathlib import Path

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
```

- [ ] **Step 9: Add scripts/.gitignore**

Write `scripts/.gitignore`:

```
.idf_path
.idf_env_cache.json
```

- [ ] **Step 10: Verify build script finds IDF**

Run: `cd /Users/briandilley/Projects/current-esp32-safemode && python scripts/build.py --skip-frontend 2>&1 | tail -20`

Expected: IDF is found, cmake configures, and the minimal main.cpp stub compiles successfully.

- [ ] **Step 11: Commit**

```bash
git add scripts/
git commit -m "build: add Python build scripts (adapted from HCU)

common.py (IDF discovery, env caching, serial port detection),
build.py, build_frontend.py, flash.py, clean.py, monitor.py,
menuconfig.py, and idf.py proxy. No encryption support.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Create OTA component

**Files:**
- Create: `firmware/components/ota/CMakeLists.txt`
- Create: `firmware/components/ota/include/ota_updater.h`
- Create: `firmware/components/ota/src/ota_updater.cpp`

- [ ] **Step 1: Create OTA component CMakeLists.txt**

Write `firmware/components/ota/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS
        "src/ota_updater.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_ota_ops
        esp_partition
)
```

- [ ] **Step 2: Create ota_updater.h**

Write `firmware/components/ota/include/ota_updater.h`:

```cpp
#pragma once

#include "esp_err.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

namespace safemode
{

class OtaUpdater
{
public:
    OtaUpdater() = default;
    ~OtaUpdater();

    OtaUpdater(const OtaUpdater&) = delete;
    OtaUpdater& operator=(const OtaUpdater&) = delete;

    /// Begin an OTA update. Finds the "app" partition and calls esp_ota_begin().
    esp_err_t begin();

    /// Write a chunk of firmware data.
    esp_err_t write(const void* data, size_t len);

    /// Finish the update. Calls esp_ota_end() and sets the boot partition.
    esp_err_t finish();

    /// Abort an in-progress update and clean up.
    void abort();

    /// Returns true if an update is in progress (begin() called, finish()/abort() not yet called).
    bool isActive() const { return active_; }

private:
    static constexpr const char* kTag = "ota";
    static constexpr const char* kAppPartitionLabel = "app";

    const esp_partition_t* partition_ = nullptr;
    esp_ota_handle_t handle_ = 0;
    bool active_ = false;
};

}  // namespace safemode
```

- [ ] **Step 3: Create ota_updater.cpp**

Write `firmware/components/ota/src/ota_updater.cpp`:

```cpp
#include "ota_updater.h"

#include "esp_log.h"

namespace safemode
{

OtaUpdater::~OtaUpdater()
{
    if (active_)
    {
        abort();
    }
}

esp_err_t OtaUpdater::begin()
{
    if (active_)
    {
        ESP_LOGW(kTag, "OTA update already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    partition_ = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, kAppPartitionLabel);
    if (partition_ == nullptr)
    {
        ESP_LOGE(kTag, "Could not find partition '%s'", kAppPartitionLabel);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(kTag, "Starting OTA update to partition '%s' @ 0x%lx (%lu bytes)",
             partition_->label, partition_->address, partition_->size);

    esp_err_t ret = esp_ota_begin(partition_, OTA_SIZE_UNKNOWN, &handle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "esp_ota_begin failed: %s", esp_err_to_name(ret));
        partition_ = nullptr;
        return ret;
    }

    active_ = true;
    return ESP_OK;
}

esp_err_t OtaUpdater::write(const void* data, size_t len)
{
    if (!active_)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_ota_write(handle_, data, len);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "esp_ota_write failed: %s", esp_err_to_name(ret));
        abort();
        return ret;
    }

    return ESP_OK;
}

esp_err_t OtaUpdater::finish()
{
    if (!active_)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_ota_end(handle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "esp_ota_end failed: %s", esp_err_to_name(ret));
        abort();
        return ret;
    }

    ret = esp_ota_set_boot_partition(partition_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(ret));
        active_ = false;
        partition_ = nullptr;
        return ret;
    }

    ESP_LOGI(kTag, "OTA update complete, boot partition set to '%s'", partition_->label);
    active_ = false;
    partition_ = nullptr;
    return ESP_OK;
}

void OtaUpdater::abort()
{
    if (active_)
    {
        esp_ota_abort(handle_);
        ESP_LOGW(kTag, "OTA update aborted");
    }
    active_ = false;
    handle_ = 0;
    partition_ = nullptr;
}

}  // namespace safemode
```

- [ ] **Step 4: Build to verify component compiles**

Run: `cd /Users/briandilley/Projects/current-esp32-safemode && python scripts/build.py --skip-frontend 2>&1 | tail -20`

Expected: Build succeeds. (main.cpp doesn't use OTA yet, but the component should be discovered and compiled by ESP-IDF's component manager.)

Note: ESP-IDF only compiles components that are depended upon. The OTA component won't be compiled until main or wifi depends on it. This is expected — it will be linked in Task 5 when main.cpp wires everything together.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/ota/
git commit -m "feat: add OTA updater component

Streaming OTA interface using esp_ota_begin/write/end APIs.
Finds the 'app' partition by label, writes firmware data in
chunks, and sets the boot partition on completion.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Create WiFi component (AP, DNS, HTTP server)

**Files:**
- Create: `firmware/components/wifi/CMakeLists.txt`
- Create: `firmware/components/wifi/include/wifi_ap.h`
- Create: `firmware/components/wifi/include/dns_server.h`
- Create: `firmware/components/wifi/include/http_server.h`
- Create: `firmware/components/wifi/src/wifi_ap.cpp`
- Create: `firmware/components/wifi/src/dns_server.cpp`
- Create: `firmware/components/wifi/src/http_server.cpp`

- [ ] **Step 1: Create WiFi component CMakeLists.txt**

Write `firmware/components/wifi/CMakeLists.txt`:

```cmake
# Include generated list of web asset files
set(WEB_EMBED_FILES "")
set(WEB_ASSETS_CMAKE "${CMAKE_CURRENT_LIST_DIR}/web_assets.cmake")
if(EXISTS "${WEB_ASSETS_CMAKE}")
    include("${WEB_ASSETS_CMAKE}")
    message(STATUS "Web assets: ${WEB_EMBED_FILES}")
else()
    message(STATUS "No web_assets.cmake found, building without frontend")
endif()

# Generate a fallback web_assets.h if it doesn't exist (e.g., CI without frontend build)
set(WEB_ASSETS_HEADER "${CMAKE_CURRENT_LIST_DIR}/src/web_assets.h")
if(NOT EXISTS "${WEB_ASSETS_HEADER}")
    file(WRITE "${WEB_ASSETS_HEADER}"
        "#pragma once\n"
        "// Auto-generated fallback — no frontend assets embedded\n"
        "#include <cstddef>\n"
        "#include <cstdint>\n"
        "\n"
        "struct WebAsset { const char* urlPath; const char* mimeType; const uint8_t* data; size_t size; bool isGzipped; };\n"
        "static constexpr int kWebAssetCount = 0;\n"
        "inline const WebAsset kWebAssets[] = {{nullptr, nullptr, nullptr, 0, false}};\n"
    )
    message(STATUS "Generated fallback web_assets.h")
endif()

idf_component_register(
    SRCS
        "src/wifi_ap.cpp"
        "src/http_server.cpp"
        "src/dns_server.cpp"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src"
    EMBED_FILES
        ${WEB_EMBED_FILES}
    REQUIRES
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
    PRIV_REQUIRES
        ota
)
```

- [ ] **Step 2: Create wifi_ap.h**

Write `firmware/components/wifi/include/wifi_ap.h`:

```cpp
#pragma once

#include "esp_err.h"

namespace safemode
{

class WifiAp
{
public:
    /// Start WiFi AP with hardcoded SSID "SAFEMODE", password "safemode", IP 4.3.2.1
    static esp_err_t start();

    /// Stop WiFi AP
    static esp_err_t stop();

private:
    static constexpr const char* kTag = "wifi_ap";
    static constexpr const char* kSsid = "SAFEMODE";
    static constexpr const char* kPassword = "safemode";
    static constexpr int kMaxConnections = 4;
    static constexpr int kChannel = 1;
};

}  // namespace safemode
```

- [ ] **Step 3: Create wifi_ap.cpp**

Write `firmware/components/wifi/src/wifi_ap.cpp`:

```cpp
#include "wifi_ap.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include <cstring>

namespace safemode
{

esp_err_t WifiAp::start()
{
    ESP_LOGI(kTag, "Starting WiFi AP: SSID=%s", kSsid);

    // Initialize network interface and event loop
    ESP_RETURN_ON_ERROR(esp_netif_init(), kTag, "netif init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), kTag, "event loop");

    // Create AP netif and configure static IP
    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();

    // Stop DHCP server to change IP
    esp_netif_dhcps_stop(ap_netif);

    esp_netif_ip_info_t ip_info = {};
    IP4_ADDR(&ip_info.ip, 4, 3, 2, 1);
    IP4_ADDR(&ip_info.gw, 4, 3, 2, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    ESP_RETURN_ON_ERROR(esp_netif_set_ip_info(ap_netif, &ip_info), kTag, "set ip");

    // Restart DHCP server with new IP range
    ESP_RETURN_ON_ERROR(esp_netif_dhcps_start(ap_netif), kTag, "dhcps start");

    // Initialize WiFi driver
    wifi_init_config_t wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifiInitConfig), kTag, "wifi init");

    // Configure AP
    wifi_config_t wifiConfig = {};
    std::strncpy(reinterpret_cast<char*>(wifiConfig.ap.ssid), kSsid, sizeof(wifiConfig.ap.ssid));
    wifiConfig.ap.ssid_len = std::strlen(kSsid);
    std::strncpy(reinterpret_cast<char*>(wifiConfig.ap.password), kPassword, sizeof(wifiConfig.ap.password));
    wifiConfig.ap.channel = kChannel;
    wifiConfig.ap.max_connection = kMaxConnections;
    wifiConfig.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), kTag, "set mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifiConfig), kTag, "set config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), kTag, "wifi start");

    ESP_LOGI(kTag, "WiFi AP started: SSID=%s, IP=4.3.2.1, channel=%d", kSsid, kChannel);
    return ESP_OK;
}

esp_err_t WifiAp::stop()
{
    ESP_LOGI(kTag, "Stopping WiFi AP");
    ESP_RETURN_ON_ERROR(esp_wifi_stop(), kTag, "wifi stop");
    ESP_RETURN_ON_ERROR(esp_wifi_deinit(), kTag, "wifi deinit");
    return ESP_OK;
}

}  // namespace safemode
```

- [ ] **Step 4: Create dns_server.h**

Write `firmware/components/wifi/include/dns_server.h`:

```cpp
#pragma once

#include "esp_err.h"

namespace safemode
{

/// Start the captive portal DNS server. All A-record queries resolve to 4.3.2.1.
esp_err_t dnsServerStart();

/// Stop the DNS server and clean up.
void dnsServerStop();

}  // namespace safemode
```

- [ ] **Step 5: Create dns_server.cpp**

Write `firmware/components/wifi/src/dns_server.cpp`:

```cpp
#include "dns_server.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#include <cstring>

namespace safemode
{

static constexpr const char* kTag = "dns";
static constexpr uint16_t kDnsPort = 53;
static constexpr size_t kDnsMaxPacket = 512;
static constexpr size_t kTaskStackSize = 4096;
static constexpr UBaseType_t kTaskPriority = 2;

// Captive portal IP: 4.3.2.1
static constexpr uint8_t kApIp[4] = {4, 3, 2, 1};

static TaskHandle_t sTask = nullptr;
static volatile bool sShouldRun = false;
static int sSock = -1;

static int buildResponse(const uint8_t* query, int queryLen, uint8_t* resp, int respMax)
{
    if (queryLen < 12 || respMax < queryLen + 16)
    {
        return -1;
    }

    std::memcpy(resp, query, queryLen);

    // Set response flags: QR=1, AA=1, RCODE=0
    resp[2] = 0x81;
    resp[3] = 0x80;

    // Set answer count to 1
    resp[6] = 0x00;
    resp[7] = 0x01;

    int pos = queryLen;

    // Name: pointer to the name in the question (offset 12)
    resp[pos++] = 0xC0;
    resp[pos++] = 0x0C;

    // Type: A (1)
    resp[pos++] = 0x00;
    resp[pos++] = 0x01;

    // Class: IN (1)
    resp[pos++] = 0x00;
    resp[pos++] = 0x01;

    // TTL: 60 seconds
    resp[pos++] = 0x00;
    resp[pos++] = 0x00;
    resp[pos++] = 0x00;
    resp[pos++] = 60;

    // RDLENGTH: 4 (IPv4)
    resp[pos++] = 0x00;
    resp[pos++] = 0x04;

    // RDATA: captive portal IP
    resp[pos++] = kApIp[0];
    resp[pos++] = kApIp[1];
    resp[pos++] = kApIp[2];
    resp[pos++] = kApIp[3];

    return pos;
}

static void dnsTask(void* arg)
{
    ESP_LOGI(kTag, "DNS server task started on port %d", kDnsPort);

    uint8_t queryBuf[kDnsMaxPacket];
    uint8_t respBuf[kDnsMaxPacket];

    while (sShouldRun)
    {
        struct sockaddr_in clientAddr = {};
        socklen_t addrLen = sizeof(clientAddr);

        int len = recvfrom(sSock, queryBuf, sizeof(queryBuf), 0,
                           reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);

        if (len < 0)
        {
            if (sShouldRun)
            {
                ESP_LOGW(kTag, "recvfrom error: %d", errno);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            continue;
        }

        int respLen = buildResponse(queryBuf, len, respBuf, sizeof(respBuf));
        if (respLen > 0)
        {
            sendto(sSock, respBuf, respLen, 0,
                   reinterpret_cast<struct sockaddr*>(&clientAddr), addrLen);
        }
    }

    ESP_LOGI(kTag, "DNS server task exiting");
    vTaskDelete(nullptr);
}

esp_err_t dnsServerStart()
{
    if (sTask)
    {
        return ESP_ERR_INVALID_STATE;
    }

    sSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sSock < 0)
    {
        ESP_LOGE(kTag, "Failed to create socket: %d", errno);
        return ESP_FAIL;
    }

    struct timeval tv = {};
    tv.tv_sec = 1;
    setsockopt(sSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kDnsPort);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sSock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        ESP_LOGE(kTag, "Failed to bind DNS socket: %d", errno);
        close(sSock);
        sSock = -1;
        return ESP_FAIL;
    }

    sShouldRun = true;
    BaseType_t created = xTaskCreate(dnsTask, "dns_srv", kTaskStackSize, nullptr, kTaskPriority, &sTask);
    if (created != pdPASS)
    {
        ESP_LOGE(kTag, "Failed to create DNS task");
        close(sSock);
        sSock = -1;
        sShouldRun = false;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(kTag, "DNS server started — all queries resolve to %d.%d.%d.%d",
             kApIp[0], kApIp[1], kApIp[2], kApIp[3]);
    return ESP_OK;
}

void dnsServerStop()
{
    if (!sTask)
        return;

    sShouldRun = false;

    if (sSock >= 0)
    {
        close(sSock);
        sSock = -1;
    }

    for (int i = 0; i < 20 && eTaskGetState(sTask) != eDeleted; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    sTask = nullptr;

    ESP_LOGI(kTag, "DNS server stopped");
}

}  // namespace safemode
```

- [ ] **Step 6: Create http_server.h**

Write `firmware/components/wifi/include/http_server.h`:

```cpp
#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace safemode
{

class OtaUpdater;

class HttpServer
{
public:
    HttpServer() = default;
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void setOtaUpdater(OtaUpdater* ota) { ota_ = ota; }

    esp_err_t start(uint16_t port = 80);
    esp_err_t stop();

    bool isRunning() const { return handle_ != nullptr; }

    /// Schedule a reboot after the given number of milliseconds.
    void scheduleReboot(uint32_t delayMs);

private:
    static constexpr const char* kTag = "http_server";
    static constexpr uint32_t kDefaultRebootDelayMs = 5000;

    httpd_handle_t handle_ = nullptr;
    OtaUpdater* ota_ = nullptr;

    void registerRoutes();

    // CORS
    static void setCorsHeaders(httpd_req_t* req);
    static esp_err_t handleOptions(httpd_req_t* req);

    // Static file serving
    static esp_err_t handleStaticFile(httpd_req_t* req);
    static esp_err_t handleSpaFallback(httpd_req_t* req);

    // API handlers
    static esp_err_t handlePing(httpd_req_t* req);
    static esp_err_t handleRestart(httpd_req_t* req);
    static esp_err_t handleBootApp(httpd_req_t* req);
    static esp_err_t handleUpdate(httpd_req_t* req);
    static esp_err_t handleInfo(httpd_req_t* req);

    // Helpers
    static void sendJsonOk(httpd_req_t* req);
    static void sendJsonError(httpd_req_t* req, int status);

    static void rebootTimerCallback(void* arg);
};

}  // namespace safemode
```

- [ ] **Step 7: Create http_server.cpp**

Write `firmware/components/wifi/src/http_server.cpp`:

```cpp
#include "http_server.h"

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "ota_updater.h"
#include "web_assets.h"

#include <cstdio>
#include <cstring>

namespace safemode
{

HttpServer::~HttpServer()
{
    stop();
}

esp_err_t HttpServer::start(uint16_t port)
{
    if (handle_)
    {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 16;
    config.recv_wait_timeout = 30;

    ESP_LOGI(kTag, "Starting HTTP server on port %d", port);
    esp_err_t ret = httpd_start(&handle_, &config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    registerRoutes();
    ESP_LOGI(kTag, "HTTP server started");
    return ESP_OK;
}

esp_err_t HttpServer::stop()
{
    if (!handle_)
        return ESP_OK;
    esp_err_t ret = httpd_stop(handle_);
    handle_ = nullptr;
    return ret;
}

// ---- JSON helpers ----

void HttpServer::sendJsonOk(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"result\":true}");
}

void HttpServer::sendJsonError(httpd_req_t* req, int status)
{
    httpd_resp_set_status(req, status == 404 ? "404 Not Found"
                             : status == 400 ? "400 Bad Request"
                                             : "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"result\":false}");
}

// ---- CORS ----

void HttpServer::setCorsHeaders(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, X-File-Size");
}

esp_err_t HttpServer::handleOptions(httpd_req_t* req)
{
    setCorsHeaders(req);
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

// ---- Reboot ----

void HttpServer::rebootTimerCallback(void* arg)
{
    ESP_LOGI("http_server", "Rebooting...");
    esp_restart();
}

void HttpServer::scheduleReboot(uint32_t delayMs)
{
    ESP_LOGI(kTag, "Scheduling reboot in %lu ms", delayMs);
    const esp_timer_create_args_t timerArgs = {
        .callback = rebootTimerCallback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "reboot",
        .skip_unhandled_events = false,
    };
    esp_timer_handle_t timer = nullptr;
    if (esp_timer_create(&timerArgs, &timer) == ESP_OK)
    {
        esp_timer_start_once(timer, static_cast<uint64_t>(delayMs) * 1000);
    }
}

// ---- Routes ----

void HttpServer::registerRoutes()
{
    // Static files from embedded web assets
    for (int i = 0; i < kWebAssetCount; i++)
    {
        httpd_uri_t uri = {};
        uri.uri = kWebAssets[i].urlPath;
        uri.method = HTTP_GET;
        uri.handler = handleStaticFile;
        uri.user_ctx = this;
        httpd_register_uri_handler(handle_, &uri);
    }

    // CORS preflight
    httpd_uri_t opts = {};
    opts.uri = "/api/*";
    opts.method = HTTP_OPTIONS;
    opts.handler = handleOptions;
    opts.user_ctx = this;
    httpd_register_uri_handler(handle_, &opts);

    // API routes
    httpd_uri_t ping = {};
    ping.uri = "/api/ping";
    ping.method = HTTP_POST;
    ping.handler = handlePing;
    ping.user_ctx = this;
    httpd_register_uri_handler(handle_, &ping);

    httpd_uri_t restart = {};
    restart.uri = "/api/restart";
    restart.method = HTTP_POST;
    restart.handler = handleRestart;
    restart.user_ctx = this;
    httpd_register_uri_handler(handle_, &restart);

    httpd_uri_t bootApp = {};
    bootApp.uri = "/api/app";
    bootApp.method = HTTP_POST;
    bootApp.handler = handleBootApp;
    bootApp.user_ctx = this;
    httpd_register_uri_handler(handle_, &bootApp);

    httpd_uri_t update = {};
    update.uri = "/api/update";
    update.method = HTTP_POST;
    update.handler = handleUpdate;
    update.user_ctx = this;
    httpd_register_uri_handler(handle_, &update);

    httpd_uri_t info = {};
    info.uri = "/api/info";
    info.method = HTTP_GET;
    info.handler = handleInfo;
    info.user_ctx = this;
    httpd_register_uri_handler(handle_, &info);

    // SPA fallback (must be last — wildcard)
    httpd_uri_t fallback = {};
    fallback.uri = "/*";
    fallback.method = HTTP_GET;
    fallback.handler = handleSpaFallback;
    fallback.user_ctx = this;
    httpd_register_uri_handler(handle_, &fallback);
}

// ---- Static Files ----

esp_err_t HttpServer::handleStaticFile(httpd_req_t* req)
{
    const char* uri = req->uri;
    const char* query = std::strchr(uri, '?');
    size_t uriLen = query ? static_cast<size_t>(query - uri) : std::strlen(uri);

    for (int i = 0; i < kWebAssetCount; i++)
    {
        if (std::strlen(kWebAssets[i].urlPath) == uriLen &&
            std::strncmp(kWebAssets[i].urlPath, uri, uriLen) == 0)
        {
            setCorsHeaders(req);
            httpd_resp_set_type(req, kWebAssets[i].mimeType);
            if (kWebAssets[i].isGzipped)
            {
                httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
            }
            const char* ext = std::strrchr(kWebAssets[i].urlPath, '.');
            if (ext && (std::strcmp(ext, ".js") == 0 || std::strcmp(ext, ".css") == 0))
            {
                httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000, immutable");
            }
            return httpd_resp_send(req, reinterpret_cast<const char*>(kWebAssets[i].data),
                                   kWebAssets[i].size);
        }
    }
    return handleSpaFallback(req);
}

esp_err_t HttpServer::handleSpaFallback(httpd_req_t* req)
{
    for (int i = 0; i < kWebAssetCount; i++)
    {
        if (std::strcmp(kWebAssets[i].urlPath, "/index.html") == 0)
        {
            httpd_resp_set_type(req, "text/html");
            if (kWebAssets[i].isGzipped)
            {
                httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
            }
            return httpd_resp_send(req, reinterpret_cast<const char*>(kWebAssets[i].data),
                                   kWebAssets[i].size);
        }
    }
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
    return ESP_OK;
}

// ---- API Handlers ----

esp_err_t HttpServer::handlePing(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: ping");
    setCorsHeaders(req);
    sendJsonOk(req);
    return ESP_OK;
}

esp_err_t HttpServer::handleRestart(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: restart");
    auto* server = static_cast<HttpServer*>(req->user_ctx);
    setCorsHeaders(req);
    sendJsonOk(req);
    server->scheduleReboot(kDefaultRebootDelayMs);
    return ESP_OK;
}

esp_err_t HttpServer::handleBootApp(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: boot into app");
    auto* server = static_cast<HttpServer*>(req->user_ctx);
    setCorsHeaders(req);

    const esp_partition_t* appPartition =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "app");
    if (appPartition == nullptr)
    {
        ESP_LOGE(kTag, "Could not find 'app' partition");
        sendJsonError(req, 500);
        return ESP_OK;
    }

    if (esp_ota_set_boot_partition(appPartition) != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to set boot partition");
        sendJsonError(req, 500);
        return ESP_OK;
    }

    sendJsonOk(req);
    server->scheduleReboot(kDefaultRebootDelayMs);
    return ESP_OK;
}

esp_err_t HttpServer::handleUpdate(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: OTA update (%d bytes)", req->content_len);
    auto* server = static_cast<HttpServer*>(req->user_ctx);
    setCorsHeaders(req);

    if (server->ota_ == nullptr)
    {
        ESP_LOGE(kTag, "OTA updater not configured");
        sendJsonError(req, 500);
        return ESP_OK;
    }

    esp_err_t ret = server->ota_->begin();
    if (ret != ESP_OK)
    {
        sendJsonError(req, 500);
        return ESP_OK;
    }

    char buf[4096];
    int remaining = req->content_len;
    int received = 0;

    while (remaining > 0)
    {
        int toRead = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
        int read = httpd_req_recv(req, buf, toRead);
        if (read <= 0)
        {
            if (read == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            ESP_LOGE(kTag, "OTA receive error");
            server->ota_->abort();
            sendJsonError(req, 500);
            return ESP_OK;
        }

        ret = server->ota_->write(buf, read);
        if (ret != ESP_OK)
        {
            sendJsonError(req, 500);
            return ESP_OK;
        }

        received += read;
        remaining -= read;
    }

    ESP_LOGI(kTag, "OTA: received %d bytes, finishing...", received);

    ret = server->ota_->finish();
    if (ret != ESP_OK)
    {
        sendJsonError(req, 500);
        return ESP_OK;
    }

    sendJsonOk(req);
    server->scheduleReboot(kDefaultRebootDelayMs);
    return ESP_OK;
}

esp_err_t HttpServer::handleInfo(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: info");
    setCorsHeaders(req);

    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);

    const esp_app_desc_t* appDesc = esp_app_get_description();
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* appPartition =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "app");

    char json[512];
    int len = snprintf(json, sizeof(json),
        "{"
        "\"chip\":\"%s\","
        "\"revision\":%d,"
        "\"cores\":%d,"
        "\"idfVersion\":\"%s\","
        "\"freeHeap\":%lu,"
        "\"runningPartition\":\"%s\","
        "\"appPartition\":\"%s\","
        "\"firmwareVersion\":\"%s\""
        "}",
        CONFIG_IDF_TARGET,
        chipInfo.revision,
        chipInfo.cores,
        esp_get_idf_version(),
        esp_get_free_heap_size(),
        running ? running->label : "unknown",
        appPartition ? appPartition->label : "none",
        appDesc ? appDesc->version : "unknown"
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, len);
    return ESP_OK;
}

}  // namespace safemode
```

- [ ] **Step 8: Build to verify WiFi component compiles (without frontend)**

Run: `cd /Users/briandilley/Projects/current-esp32-safemode && python scripts/build.py --skip-frontend 2>&1 | tail -20`

Note: The wifi component won't be compiled until main.cpp depends on it. This build just verifies the existing main stub still works. The full component will be exercised in Task 5.

- [ ] **Step 9: Commit**

```bash
git add firmware/components/wifi/
git commit -m "feat: add WiFi component (AP, DNS captive portal, HTTP server)

WiFi AP on 4.3.2.1 with SSID 'SAFEMODE', captive portal DNS
server resolving all queries to AP IP, and esp_http_server with
REST API routes (ping, restart, boot-app, OTA update, device info),
embedded web asset serving with gzip and cache headers, and SPA
fallback for client-side routing.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: Wire everything together in main.cpp

**Files:**
- Modify: `firmware/main/CMakeLists.txt`
- Modify: `firmware/main/main.cpp`

- [ ] **Step 1: Update main/CMakeLists.txt to depend on components**

Write `firmware/main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        wifi
        ota
    PRIV_REQUIRES
        nvs_flash
        esp_timer
)
```

- [ ] **Step 2: Write the full main.cpp**

Write `firmware/main/main.cpp`:

```cpp
#include "dns_server.h"
#include "esp_log.h"
#include "http_server.h"
#include "nvs_flash.h"
#include "ota_updater.h"
#include "wifi_ap.h"

static constexpr const char* kTag = "safemode";

extern "C" void app_main()
{
    ESP_LOGI(kTag, "Safemode starting...");

    // Quiet noisy ESP-IDF components
    esp_log_level_set("httpd", ESP_LOG_WARN);
    esp_log_level_set("httpd_uri", ESP_LOG_WARN);
    esp_log_level_set("httpd_txrx", ESP_LOG_WARN);
    esp_log_level_set("httpd_parse", ESP_LOG_WARN);
    esp_log_level_set("httpd_sess", ESP_LOG_WARN);
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("wifi_init", ESP_LOG_WARN);
    esp_log_level_set("esp_netif_lwip", ESP_LOG_WARN);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(kTag, "NVS partition truncated, erasing...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return;
    }

    // Start WiFi AP
    ret = safemode::WifiAp::start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to start WiFi AP: %s", esp_err_to_name(ret));
        return;
    }

    // Start DNS server (captive portal)
    ret = safemode::dnsServerStart();
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to start DNS server: %s", esp_err_to_name(ret));
    }

    // Create OTA updater
    safemode::OtaUpdater otaUpdater;

    // Start HTTP server
    safemode::HttpServer httpServer;
    httpServer.setOtaUpdater(&otaUpdater);

    ret = httpServer.start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(kTag, "Safemode ready — connect to WiFi 'SAFEMODE' (password: safemode)");
    ESP_LOGI(kTag, "Open http://4.3.2.1 in your browser");

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

- [ ] **Step 3: Build the complete firmware (without frontend)**

Run: `cd /Users/briandilley/Projects/current-esp32-safemode && python scripts/build.py --skip-frontend 2>&1 | tail -30`

Expected: Build succeeds. All three components (ota, wifi, main) compile and link. The fallback `web_assets.h` is auto-generated by the wifi CMakeLists.txt with an empty assets table.

- [ ] **Step 4: Verify binary size**

Run: `ls -la /Users/briandilley/Projects/current-esp32-safemode/firmware/build/safemode.bin`

Expected: Binary exists and is well under 1.7MB (safemode partition size is 0x1B0000 = 1,769,472 bytes). Without the frontend, it should be around 200-400KB.

- [ ] **Step 5: Commit**

```bash
git add firmware/main/
git commit -m "feat: wire main.cpp to all components

Initialize NVS, start WiFi AP, DNS captive portal, OTA updater,
and HTTP server. Device is now fully functional via REST API
(without web UI frontend).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Create React frontend

**Files:**
- Create: `firmware/frontend/package.json`
- Create: `firmware/frontend/tsconfig.json`
- Create: `firmware/frontend/tsconfig.app.json`
- Create: `firmware/frontend/vite.config.ts`
- Create: `firmware/frontend/index.html`
- Create: `firmware/frontend/.nvmrc`
- Create: `firmware/frontend/.gitignore`
- Create: `firmware/frontend/src/main.tsx`
- Create: `firmware/frontend/src/index.css`
- Create: `firmware/frontend/src/api.ts`
- Create: `firmware/frontend/src/App.tsx`

- [ ] **Step 1: Create package.json**

Write `firmware/frontend/package.json`:

```json
{
  "name": "safemode-frontend",
  "private": true,
  "version": "0.1.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "tsc -b && vite build",
    "preview": "vite preview",
    "typecheck": "tsc --noEmit",
    "lint": "eslint src/"
  },
  "dependencies": {
    "react": "^19.2.4",
    "react-dom": "^19.2.4"
  },
  "devDependencies": {
    "@tailwindcss/vite": "^4.2.2",
    "@types/react": "^19.2.14",
    "@types/react-dom": "^19.2.3",
    "@vitejs/plugin-react": "^6.0.1",
    "tailwindcss": "^4.2.2",
    "typescript": "^6.0.2",
    "vite": "^8.0.7"
  }
}
```

- [ ] **Step 2: Create tsconfig.json**

Write `firmware/frontend/tsconfig.json`:

```json
{
  "compilerOptions": {
    "target": "ES2020",
    "useDefineForClassFields": true,
    "lib": ["ES2020", "DOM", "DOM.Iterable"],
    "module": "ESNext",
    "skipLibCheck": true,
    "moduleResolution": "bundler",
    "allowImportingTsExtensions": true,
    "isolatedModules": true,
    "moduleDetection": "force",
    "noEmit": true,
    "jsx": "react-jsx",
    "strict": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "noFallthroughCasesInSwitch": true,
    "noUncheckedSideEffectImports": true
  },
  "include": ["src"]
}
```

Write `firmware/frontend/tsconfig.app.json`:

```json
{
  "extends": "./tsconfig.json",
  "include": ["src"]
}
```

- [ ] **Step 3: Create vite.config.ts**

Write `firmware/frontend/vite.config.ts`:

```typescript
import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";

export default defineConfig({
  plugins: [react(), tailwindcss()],
  build: {
    outDir: "dist",
    emptyOutDir: true,
  },
  server: {
    proxy: {
      "/api": {
        target: "http://4.3.2.1",
        changeOrigin: true,
      },
    },
  },
});
```

- [ ] **Step 4: Create index.html**

Write `firmware/frontend/index.html`:

```html
<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Safemode</title>
  </head>
  <body>
    <div id="root"></div>
    <script type="module" src="/src/main.tsx"></script>
  </body>
</html>
```

- [ ] **Step 5: Create .nvmrc and .gitignore**

Write `firmware/frontend/.nvmrc`:

```
22
```

Write `firmware/frontend/.gitignore`:

```
node_modules
dist
*.local
```

- [ ] **Step 6: Create src/index.css**

Write `firmware/frontend/src/index.css`:

```css
@import "tailwindcss";

body {
  font-family: system-ui, -apple-system, sans-serif;
  margin: 0;
  min-height: 100dvh;
  background-color: #1c1b1f;
  color: #e6e1e5;
}
```

- [ ] **Step 7: Create src/api.ts**

Write `firmware/frontend/src/api.ts`:

```typescript
export interface DeviceInfo {
  chip: string;
  revision: number;
  cores: number;
  idfVersion: string;
  freeHeap: number;
  runningPartition: string;
  appPartition: string;
  firmwareVersion: string;
}

function apiUrl(path: string): string {
  return path;
}

export async function ping(): Promise<boolean> {
  try {
    const res = await fetch(apiUrl("/api/ping"), {
      method: "POST",
      signal: AbortSignal.timeout(5000),
    });
    const json = await res.json();
    return json.result === true;
  } catch {
    return false;
  }
}

export async function restart(): Promise<boolean> {
  const res = await fetch(apiUrl("/api/restart"), { method: "POST" });
  const json = await res.json();
  return json.result === true;
}

export async function bootIntoApp(): Promise<boolean> {
  const res = await fetch(apiUrl("/api/app"), { method: "POST" });
  const json = await res.json();
  return json.result === true;
}

export function uploadFirmware(
  file: File,
  onProgress?: (pct: number) => void
): Promise<boolean> {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open("POST", apiUrl("/api/update"));
    xhr.setRequestHeader("X-File-Size", String(file.size));

    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable && onProgress) {
        onProgress(Math.round((e.loaded / e.total) * 100));
      }
    };

    xhr.onload = () => {
      try {
        const json = JSON.parse(xhr.responseText);
        resolve(json.result === true);
      } catch {
        resolve(false);
      }
    };

    xhr.onerror = () => reject(new Error("Upload failed"));
    xhr.ontimeout = () => reject(new Error("Upload timed out"));
    xhr.timeout = 120000;

    xhr.send(file);
  });
}

export async function getInfo(): Promise<DeviceInfo> {
  const res = await fetch(apiUrl("/api/info"));
  return res.json();
}
```

- [ ] **Step 8: Create src/main.tsx**

Write `firmware/frontend/src/main.tsx`:

```tsx
import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import App from "./App";
import "./index.css";

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
  </StrictMode>
);
```

- [ ] **Step 9: Create src/App.tsx**

Write `firmware/frontend/src/App.tsx`:

```tsx
import { useCallback, useEffect, useRef, useState } from "react";
import {
  ping,
  restart,
  bootIntoApp,
  uploadFirmware,
  getInfo,
  type DeviceInfo,
} from "./api";

function StatusDot({ connected }: { connected: boolean }) {
  return (
    <span
      className={`inline-block h-2.5 w-2.5 rounded-full ${
        connected ? "bg-green-400" : "bg-red-400"
      }`}
    />
  );
}

export default function App() {
  const [connected, setConnected] = useState(false);
  const [file, setFile] = useState<File | null>(null);
  const [uploading, setUploading] = useState(false);
  const [progress, setProgress] = useState(0);
  const [message, setMessage] = useState<{
    text: string;
    type: "success" | "error";
  } | null>(null);
  const [deviceInfo, setDeviceInfo] = useState<DeviceInfo | null>(null);
  const [showInfo, setShowInfo] = useState(false);
  const fileInputRef = useRef<HTMLInputElement>(null);

  // Poll connectivity
  useEffect(() => {
    let active = true;
    const poll = async () => {
      while (active) {
        const ok = await ping();
        if (active) setConnected(ok);
        await new Promise((r) => setTimeout(r, 5000));
      }
    };
    poll();
    return () => {
      active = false;
    };
  }, []);

  // Fetch device info when connected
  useEffect(() => {
    if (connected) {
      getInfo()
        .then(setDeviceInfo)
        .catch(() => setDeviceInfo(null));
    }
  }, [connected]);

  const handleUpload = useCallback(async () => {
    if (!file) return;
    setUploading(true);
    setProgress(0);
    setMessage(null);

    try {
      const ok = await uploadFirmware(file, setProgress);
      if (ok) {
        setMessage({ text: "Update successful! Device is rebooting...", type: "success" });
        setFile(null);
        if (fileInputRef.current) fileInputRef.current.value = "";
      } else {
        setMessage({ text: "Update failed. Please try again.", type: "error" });
      }
    } catch {
      setMessage({ text: "Upload error. Check your connection.", type: "error" });
    } finally {
      setUploading(false);
    }
  }, [file]);

  const handleLeaveSafemode = useCallback(async () => {
    if (!confirm("Are you sure you want to leave safe mode and boot into the app?")) return;
    const ok = await bootIntoApp();
    if (ok) {
      setMessage({ text: "Device is rebooting into app...", type: "success" });
    } else {
      setMessage({ text: "Failed to switch partition.", type: "error" });
    }
  }, []);

  const handleRestart = useCallback(async () => {
    const ok = await restart();
    if (ok) {
      setMessage({ text: "Device is restarting...", type: "success" });
    } else {
      setMessage({ text: "Failed to restart device.", type: "error" });
    }
  }, []);

  return (
    <div className="mx-auto flex min-h-dvh max-w-lg flex-col px-4 py-6">
      {/* Header */}
      <header className="mb-6 flex items-center justify-between">
        <h1 className="text-2xl font-bold tracking-tight text-orange-400">SAFEMODE</h1>
        <div className="flex items-center gap-2 text-sm text-stone-400">
          <StatusDot connected={connected} />
          {connected ? "Connected" : "Disconnected"}
        </div>
      </header>

      {/* Message */}
      {message && (
        <div
          className={`mb-4 rounded-lg px-4 py-3 text-sm ${
            message.type === "success"
              ? "bg-green-900/30 text-green-300"
              : "bg-red-900/30 text-red-300"
          }`}
        >
          {message.text}
        </div>
      )}

      {/* OTA Update */}
      <section className="mb-6 rounded-xl bg-stone-800/50 p-5">
        <h2 className="mb-3 text-lg font-semibold text-stone-200">Firmware Update</h2>
        <p className="mb-4 text-sm text-stone-400">
          Select a firmware image (.bin) to upload. The device will reboot after updating.
        </p>

        <input
          ref={fileInputRef}
          type="file"
          accept=".bin"
          onChange={(e) => setFile(e.target.files?.[0] ?? null)}
          disabled={uploading}
          className="mb-4 block w-full text-sm text-stone-400 file:mr-4 file:rounded-lg file:border-0 file:bg-stone-700 file:px-4 file:py-2 file:text-sm file:font-medium file:text-stone-200 hover:file:bg-stone-600"
        />

        {uploading && (
          <div className="mb-4">
            <div className="mb-1 flex justify-between text-xs text-stone-400">
              <span>Uploading...</span>
              <span>{progress}%</span>
            </div>
            <div className="h-2 overflow-hidden rounded-full bg-stone-700">
              <div
                className="h-full rounded-full bg-orange-400 transition-all duration-300"
                style={{ width: `${progress}%` }}
              />
            </div>
          </div>
        )}

        <button
          onClick={handleUpload}
          disabled={!file || uploading || !connected}
          className="w-full rounded-lg bg-orange-500 px-4 py-2.5 text-sm font-semibold text-white transition-colors hover:bg-orange-400 disabled:cursor-not-allowed disabled:opacity-40"
        >
          {uploading ? "Uploading..." : "Update Firmware"}
        </button>
      </section>

      {/* Actions */}
      <section className="mb-6 rounded-xl bg-stone-800/50 p-5">
        <h2 className="mb-3 text-lg font-semibold text-stone-200">Actions</h2>
        <div className="flex gap-3">
          <button
            onClick={handleLeaveSafemode}
            disabled={uploading || !connected}
            className="flex-1 rounded-lg bg-stone-700 px-4 py-2.5 text-sm font-medium text-stone-200 transition-colors hover:bg-stone-600 disabled:cursor-not-allowed disabled:opacity-40"
          >
            Leave Safemode
          </button>
          <button
            onClick={handleRestart}
            disabled={uploading || !connected}
            className="flex-1 rounded-lg bg-stone-700 px-4 py-2.5 text-sm font-medium text-stone-200 transition-colors hover:bg-stone-600 disabled:cursor-not-allowed disabled:opacity-40"
          >
            Restart
          </button>
        </div>
      </section>

      {/* Device Info */}
      <section className="rounded-xl bg-stone-800/50">
        <button
          onClick={() => setShowInfo(!showInfo)}
          className="flex w-full items-center justify-between px-5 py-3 text-sm font-medium text-stone-400 transition-colors hover:text-stone-200"
        >
          <span>Device Info</span>
          <span className={`transition-transform ${showInfo ? "rotate-180" : ""}`}>
            &#9660;
          </span>
        </button>
        {showInfo && deviceInfo && (
          <div className="border-t border-stone-700/50 px-5 py-3">
            <dl className="grid grid-cols-2 gap-x-4 gap-y-2 text-sm">
              <dt className="text-stone-500">Chip</dt>
              <dd className="text-stone-300">{deviceInfo.chip}</dd>
              <dt className="text-stone-500">Cores</dt>
              <dd className="text-stone-300">{deviceInfo.cores}</dd>
              <dt className="text-stone-500">IDF Version</dt>
              <dd className="text-stone-300">{deviceInfo.idfVersion}</dd>
              <dt className="text-stone-500">Free Heap</dt>
              <dd className="text-stone-300">{(deviceInfo.freeHeap / 1024).toFixed(0)} KB</dd>
              <dt className="text-stone-500">Running</dt>
              <dd className="text-stone-300">{deviceInfo.runningPartition}</dd>
              <dt className="text-stone-500">App Partition</dt>
              <dd className="text-stone-300">{deviceInfo.appPartition}</dd>
              <dt className="text-stone-500">FW Version</dt>
              <dd className="text-stone-300">{deviceInfo.firmwareVersion}</dd>
            </dl>
          </div>
        )}
        {showInfo && !deviceInfo && (
          <div className="border-t border-stone-700/50 px-5 py-3 text-sm text-stone-500">
            Connect to device to view info
          </div>
        )}
      </section>
    </div>
  );
}
```

- [ ] **Step 10: Install dependencies and verify frontend builds**

```bash
cd /Users/briandilley/Projects/current-esp32-safemode/firmware/frontend && npm install && npm run build
```

Expected: TypeScript compiles, Vite builds, `dist/` directory created with `index.html` and hashed asset files.

- [ ] **Step 11: Commit**

```bash
cd /Users/briandilley/Projects/current-esp32-safemode
git add firmware/frontend/
git commit -m "feat: add React 19 + Tailwind v4 frontend

Single-page OTA updater UI with firmware upload (progress bar),
leave-safemode and restart buttons, connectivity polling, and
collapsible device info panel. Dark theme, mobile-first.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: Full build integration and verification

**Files:**
- No new files — this task verifies the full pipeline works end-to-end.

- [ ] **Step 1: Run full build (frontend + firmware)**

```bash
cd /Users/briandilley/Projects/current-esp32-safemode && python scripts/build.py 2>&1 | tail -40
```

Expected: Frontend builds, assets are gzipped and embedded, firmware compiles and links with embedded web assets.

- [ ] **Step 2: Verify binary fits in partition**

```bash
ls -la /Users/briandilley/Projects/current-esp32-safemode/firmware/build/safemode.bin
```

Expected: Binary size is well under 1,769,472 bytes (1.7MB safemode partition).

- [ ] **Step 3: Verify web assets were generated**

```bash
ls -la /Users/briandilley/Projects/current-esp32-safemode/firmware/components/wifi/web/
head -30 /Users/briandilley/Projects/current-esp32-safemode/firmware/components/wifi/src/web_assets.h
cat /Users/briandilley/Projects/current-esp32-safemode/firmware/components/wifi/web_assets.cmake
```

Expected: `web/` directory contains gzipped assets, `web_assets.h` has extern declarations and `kWebAssets[]` table, `web_assets.cmake` lists all embedded files.

- [ ] **Step 4: Add generated files to gitignore**

Verify `firmware/.gitignore` excludes build artifacts but the generated web assets in `components/wifi/web/` and `components/wifi/src/web_assets.h` and `components/wifi/web_assets.cmake` should also be gitignored since they are generated by the build.

Edit `firmware/.gitignore` to add:

```
build/
build-release/
sdkconfig
sdkconfig.old
managed_components/
.idea/
components/wifi/web/
components/wifi/src/web_assets.h
components/wifi/web_assets.cmake
```

- [ ] **Step 5: Commit**

```bash
git add firmware/.gitignore
git commit -m "build: gitignore generated web assets

web_assets.h, web_assets.cmake, and components/wifi/web/ are
generated by build_frontend.py and should not be committed.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: Update CLAUDE.md and write refactor loop plan

**Files:**
- Modify: `CLAUDE.md`
- Create: `.claude/refactor-loop-plan.md`

- [ ] **Step 1: Update CLAUDE.md**

Rewrite `CLAUDE.md` to reflect the new architecture:

```markdown
# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 "safemode" firmware — a captive-portal recovery partition that lets users OTA-flash new application firmware over WiFi. The device exposes an AP ("SAFEMODE"), redirects all DNS to itself, and serves a React-based upload UI.

Two-partition boot scheme: `safemode` (ota_0) is the always-present recovery image; `app` (ota_1) is the main application. The safemode firmware writes new binaries to the `app` partition via ESP-IDF OTA APIs and reboots into it.

## Hardware

ESP32-WROOM-32E-N8R2 (dual-core Xtensa LX6 @ 240MHz, 8MB flash, 2MB PSRAM)

## Build Commands

All commands from repo root:

```sh
# Full build (frontend + firmware)
python scripts/build.py

# Firmware only (skip frontend)
python scripts/build.py --skip-frontend

# Release build
python scripts/build.py --release

# Flash to device
python scripts/flash.py

# Flash app partition only (fast iteration)
python scripts/flash.py --app-only

# Flash and open serial monitor
python scripts/flash.py --monitor

# Serial monitor only
python scripts/monitor.py

# Clean build artifacts
python scripts/clean.py

# ESP-IDF menuconfig
python scripts/menuconfig.py

# Direct idf.py proxy
python scripts/idf.py <args>
```

Frontend dev server (from `firmware/frontend/`):
```sh
npm run dev      # Vite dev server, proxies /api to 4.3.2.1
npm run build    # TypeScript check + Vite production build
```

## Architecture

Pure ESP-IDF 6.0, C++20, `safemode::` namespace. No Arduino, no PlatformIO.

### Components

- **`components/ota/`** — `OtaUpdater` class: streaming OTA via `esp_ota_begin/write/end`. Finds "app" partition by label.
- **`components/wifi/`** — WiFi AP (`WifiAp`), DNS captive portal (`dnsServerStart/Stop`), HTTP server (`HttpServer`) with REST API + embedded web assets.

### main/main.cpp

Wires everything: NVS init, WiFi AP start (SSID "SAFEMODE", pass "safemode", IP 4.3.2.1), DNS server start, OTA updater + HTTP server start, idle loop.

### API Endpoints

- `POST /api/ping` — health check
- `POST /api/restart` — schedule reboot (5s delay)
- `POST /api/app` — set boot partition to "app", reboot
- `POST /api/update` — receive firmware binary, stream to OTA updater
- `GET /api/info` — device metadata (chip, IDF version, heap, partitions)
- `OPTIONS /api/*` — CORS preflight

### Frontend

React 19 + Tailwind CSS v4 + Vite 8 + TypeScript. Located at `firmware/frontend/`. Built by `scripts/build_frontend.py`, which gzip-compresses assets and generates `web_assets.h` + `web_assets.cmake` for ESP-IDF `EMBED_FILES`.

### Flash Partition Layout

| Name     | Type | Offset     | Size    |
|----------|------|------------|---------|
| nvs      | data | 0x11000    | 52KB    |
| otadata  | data | 0x1E000    | 8KB     |
| safemode | app  | 0x20000    | 1.7MB   |
| app      | app  | 0x1D0000   | 1.7MB   |
| spiffs   | data | 0x380000   | 488KB   |

## Code Style

- `safemode::` namespace, PascalCase classes, camelCase methods, `kConstantName` constants
- Allman brace style
- `static constexpr const char* kTag = "component_name"` for ESP_LOG tags
- No singletons — explicit construction wired in main.cpp
- `StaticFiles.h` / `web_assets.h` / `web_assets.cmake` are generated — do not edit manually
```

- [ ] **Step 2: Create .claude/refactor-loop-plan.md**

Create directory `.claude/` and write `.claude/refactor-loop-plan.md` with the refactor loop plan content from the design spec's "Refactor Loop Plan" section. This file serves as the self-contained runbook for the iterative refactor loop.

Write `.claude/refactor-loop-plan.md`:

```markdown
# Refactor Loop Plan

This document is a complete, self-contained runbook for iteratively verifying and fixing the ESP-IDF safemode firmware rewrite. A fresh Claude instance should be able to read this file and execute the loop without any other context.

## Project Context

ESP32 safemode recovery firmware. Rewritten from PlatformIO/Arduino to ESP-IDF 6.0 component architecture.

- **Source of truth for functionality**: The design spec at `docs/superpowers/specs/2026-04-21-esp-idf-refactor-design.md`
- **Build command**: `python scripts/build.py`
- **Namespace**: `safemode::`
- **Language**: C++20, pure ESP-IDF (no Arduino, no PlatformIO)
- **Hardware**: ESP32-WROOM-32E-N8R2 (dual-core Xtensa LX6 @ 240MHz, 8MB flash, 2MB PSRAM)
- **Code style**: PascalCase classes, camelCase methods, `kConstantName` constants, Allman braces

## Component Layout

| Component | Purpose |
|-----------|---------|
| `components/ota/` | OtaUpdater: esp_ota_begin/write/end streaming OTA |
| `components/wifi/` | WiFi AP, DNS captive portal, HTTP server, web assets |
| `main/main.cpp` | Wires NVS, WiFi, DNS, OTA, HTTP together |
| `frontend/` | React 19 + Tailwind v4 SPA |

## The Loop

### Loop Structure

Repeat until Final Verification confirms REFACTOR_TRULY_COMPLETE:

```
ITERATION N:
  Step 1: PLAN          — Opus agent audits current state vs spec, writes .claude/refactor-plan.md
  Step 1b: VERIFY       — (only on REFACTOR_COMPLETE) Fresh Opus agent independently verifies
  Step 2: EXECUTE       — Opus agent implements the plan
  Step 3: BUILD         — python scripts/build.py, fix if needed
  Step 4: COMMIT        — Auto-commit
  Step 5: UPDATE DOCS   — Update CLAUDE.md, .claude/memory/ if needed
```

### Step 1: PLAN

Spawn an agent with `subagent_type: "general-purpose"` and `model: "opus"`. Research only — writes `.claude/refactor-plan.md`, no code changes.

**Agent prompt:**

```
You are auditing an ESP32 safemode firmware rewrite. Compare the current implementation against the design spec to find any missing functionality.

Read the design spec at docs/superpowers/specs/2026-04-21-esp-idf-refactor-design.md completely.

Then audit the current implementation:
1. Read every file in firmware/components/ota/
2. Read every file in firmware/components/wifi/
3. Read firmware/main/main.cpp
4. Read every file in firmware/frontend/src/
5. Check that scripts/ contains all required scripts and they match the spec
6. Verify partitions.csv and sdkconfig.defaults match the spec

For each spec requirement, verify the implementation exists and is correct.

## Output

### If you find gaps:
Write a plan to .claude/refactor-plan.md listing each gap with:
- What's missing (cite spec section)
- Which files to modify/create
- Implementation steps

### If everything matches the spec:
Write REFACTOR_COMPLETE to .claude/refactor-plan.md

## Rules
- Actually READ the code and the spec
- Only write .claude/refactor-plan.md
- Be specific — cite file paths and function names
- Cosmetic differences are NOT gaps
```

**After:** Read `.claude/refactor-plan.md`. If `REFACTOR_COMPLETE`, proceed to Step 1b. Otherwise proceed to Step 2.

### Step 1b: FINAL VERIFICATION

Spawn a fresh agent with `subagent_type: "general-purpose"` and `model: "opus"`.

**Agent prompt:**

```
You are a FINAL VERIFICATION agent for a firmware rewrite. A previous agent declared the rewrite complete. Independently verify this.

Read the design spec at docs/superpowers/specs/2026-04-21-esp-idf-refactor-design.md.
Read ALL implementation files in firmware/components/, firmware/main/, firmware/frontend/src/, and scripts/.

Verify every spec requirement is implemented. Check:
- All API endpoints exist with correct methods and response formats
- WiFi AP config matches spec (SSID, password, IP)
- DNS server resolves to correct IP
- OTA updater uses esp_ota_begin/write/end (not raw flash ops)
- Frontend has all UI elements described in spec
- Build scripts match spec (no encryption, correct addresses)
- sdkconfig.defaults has all required settings

### If gaps found:
Write plan to .claude/refactor-plan.md with GAPS_FOUND header.

### If everything verified:
Write REFACTOR_TRULY_COMPLETE to .claude/refactor-plan.md.
```

### Step 2: EXECUTE

Spawn agent with `subagent_type: "general-purpose"` and `model: "opus"`.

**Agent prompt:**

```
Read .claude/refactor-plan.md and implement the changes described.

Context:
- ESP-IDF 6.0, C++20, safemode:: namespace
- Allman braces, PascalCase classes, camelCase methods, kConstant names
- No Arduino, no PlatformIO
- Do NOT build, commit, or modify docs

Follow the plan precisely.
```

### Step 3: BUILD

```bash
cd /Users/briandilley/Projects/current-esp32-safemode && python scripts/build.py 2>&1 | tail -100
```

If build fails, spawn a fix agent with the errors. Max 3 retries.

### Step 4: COMMIT

```bash
git add firmware/ scripts/
git commit -m "refactor: [description from plan]

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

### Step 5: UPDATE DOCS

Update CLAUDE.md and .claude/memory/ if changes affect documented architecture.

## Completion Checklist

**Infrastructure:**
- [ ] PlatformIO files removed
- [ ] Arduino dependencies removed
- [ ] Python scripts/ created and functional
- [ ] CMakeLists.txt files created
- [ ] sdkconfig.defaults created
- [ ] partitions.csv preserved

**Components:**
- [ ] WiFi AP (SSID "SAFEMODE", pass "safemode", IP 4.3.2.1)
- [ ] DNS captive portal (all queries -> 4.3.2.1)
- [ ] HTTP server with static file serving + SPA fallback
- [ ] API routes (ping, restart, app, update, info)
- [ ] CORS middleware
- [ ] web_assets.h / web_assets.cmake integration
- [ ] OtaUpdater with begin/write/finish/abort
- [ ] main.cpp wires everything together

**Frontend:**
- [ ] React 19 + Tailwind v4 + Vite 8
- [ ] OTA upload with progress
- [ ] Leave Safemode with confirmation
- [ ] Restart button
- [ ] Connectivity indicator
- [ ] Device info section
- [ ] Builds and embeds via build_frontend.py

**Integration:**
- [ ] python scripts/build.py succeeds
- [ ] Binary fits in safemode partition (1.7MB)
- [ ] Old code fully removed

## Completion Criteria

1. Planner declares REFACTOR_COMPLETE
2. Final Verification confirms REFACTOR_TRULY_COMPLETE
3. Functional parity with old firmware
4. Firmware builds and fits in partition
5. No TODO stubs remaining
```

- [ ] **Step 3: Commit**

```bash
git add CLAUDE.md .claude/
git commit -m "docs: update CLAUDE.md and add refactor loop plan

Rewrite CLAUDE.md for new ESP-IDF architecture. Add refactor loop
plan for iterative verification of the rewrite.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```
