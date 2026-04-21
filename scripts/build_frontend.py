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

# Extensions that benefit from gzip compression (text-based formats)
GZIP_EXTENSIONS = {".html", ".js", ".css", ".svg", ".json", ".map"}


def find_nvm_node() -> str | None:
    """Find nvm and return the path to use node via nvm."""
    nvm_dir = os.environ.get("NVM_DIR", str(Path.home() / ".nvm"))
    nvm_sh = Path(nvm_dir) / "nvm.sh"
    if nvm_sh.exists():
        return str(nvm_sh)
    return None


def _check_node_available() -> bool:
    """Check if node/npm are available on PATH (without nvm)."""
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
        print("[safemode]   nvm install   # reads .nvmrc in frontend/", file=sys.stderr)
        print("[safemode]", file=sys.stderr)
        print("[safemode] Windows — install nvm-windows (recommended):", file=sys.stderr)
        print("[safemode]   https://github.com/coreybutler/nvm-windows/releases", file=sys.stderr)
        print("[safemode]   nvm install 22.15.1", file=sys.stderr)
        print("[safemode]   nvm use 22.15.1", file=sys.stderr)
        print("[safemode]", file=sys.stderr)
        print("[safemode] Or install Node.js directly: https://nodejs.org/", file=sys.stderr)
        return 1

    if nvm_sh:
        # Use nvm to ensure correct node version
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
    """Convert a file path to a valid C symbol name.

    ESP-IDF EMBED_FILES generates symbols like:
      _binary_<path_with_underscores>_start
      _binary_<path_with_underscores>_end
    where slashes, dots, and hyphens become underscores.
    """
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
        # URL path as served by the web server
        url_path = "/" + str(rel_path).replace("\\", "/")
        # Path relative to web/ dir (for CMake EMBED_FILES)
        embed_path = "web/" + str(rel_path).replace("\\", "/")
        # Symbol name: ESP-IDF uses only the FILENAME (not path) for symbols
        sym = symbol_name(path.name)
        # MIME type
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
    # Gzip-compress text-based assets in-place for smaller binary embedding
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

    # Extern declarations for each file
    for f in files:
        sym = f["symbol"]
        lines.append(f'extern const uint8_t {sym}_start[] asm("_binary_{sym}_start");')
        lines.append(f'extern const uint8_t {sym}_end[] asm("_binary_{sym}_end");')
        lines.append("")

    # Asset table struct (Allman braces to match project .clang-format)
    lines.append("struct WebAsset")
    lines.append("{")
    lines.append("    const char* urlPath;")
    lines.append("    const char* mimeType;")
    lines.append("    const uint8_t* data;")
    lines.append("    size_t size;")
    lines.append("    bool isGzipped;")
    lines.append("};")
    lines.append("")

    # Asset table
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
