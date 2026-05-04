# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 "safemode" firmware — a universal recovery partition that works on any ESP32 device. Boots into a WiFi AP, serves a React-based web UI for OTA firmware updates, and optionally provides factory reset with selective NVS key preservation.

Partition-agnostic: scans flash at startup to discover the partition table (no hardcoded offsets). Supports flash encryption (pre-encrypted `.enc.bin` uploads). NVS is best-effort — WiFi uses RAM-only storage as fallback.

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

# Clean everything (all builds, caches, generated files)
python scripts/clean.py --all

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

### Startup Sequence (main.cpp)

1. Scan flash for partition table (always — no compiled-in offset dependency)
2. Register discovered partitions via `esp_partition_register_external()`
3. Init NVS (best-effort — continue without it if unavailable)
4. Start WiFi AP (SSID "SAFEMODE", pass "safemode", IP 4.3.2.1, RAM-only storage)
5. Start HTTP server with OTA updater
6. Idle loop

### Components

- **`components/partition_scan/`** — Scans flash 0x4000–0x20000 for partition table magic (0x50AA). Handles flash encryption via `esp_flash_read_encrypted()`. Registers all found partitions with ESP-IDF.
- **`components/ota/`** — `OtaUpdater` class: erases target partition, writes data via `esp_partition_write()` (plaintext) or `esp_partition_write_raw()` (pre-encrypted), sets boot partition via `esp_ota_set_boot_partition()`. Does NOT use `esp_ota_begin/write/end` (incompatible with externally-registered partitions).
- **`components/factory_reset/`** — Reads `safemode:frEnabled` and `safemode:frPreserve` from NVS. Backs up typed key/value pairs, erases NVS, restores preserved keys. Preserve list format: `namespace:key:type` (comma-separated). NVS key names are capped at 15 chars (`NVS_KEY_NAME_MAX_SIZE - 1`); longer names like `factoryResetEnabled` are silently rejected as `ESP_ERR_NVS_KEY_TOO_LONG`.
- **`components/wifi/`** — WiFi AP (`WifiAp`, RAM-only storage), HTTP server (`HttpServer`) with REST API + embedded web assets.
- **`components/ble_info/`** — NimBLE-based info broadcaster. Service `5afe0000-2026-4d3e-b9c1-7fa8c4d6e8a1` with four read-only characteristics returning plain UTF-8 strings: SSID (`5afe0001`), Password (`5afe0002`), IP (`5afe0003`), Version (`5afe0004`). No pairing.

### API Endpoints

- `POST /api/ping` — health check
- `POST /api/restart` — schedule reboot (5s delay)
- `POST /api/app` — set boot partition to app (first non-safemode app partition), reboot
- `POST /api/update` — receive firmware binary, stream to OTA updater
- `POST /api/factory-reset` — wipe NVS with key preservation
- `GET /api/info` — device metadata (chip, IDF version, heap, partitions, factoryResetEnabled)
- `OPTIONS /api/*` — CORS preflight

### Frontend

React 19 + Tailwind CSS v4 + Vite 8 + TypeScript. Located at `firmware/frontend/`. Built by `scripts/build_frontend.py`, which gzip-compresses assets and generates `web_assets.h` + `web_assets.cmake` for ESP-IDF `EMBED_FILES`.

### NVS Convention

Host apps configure safemode by writing to the `"safemode"` NVS namespace:

| Key | Type | Description |
|-----|------|-------------|
| `frEnabled` | `u8` | Set to `1` to show factory reset button |
| `frPreserve` | `str` | Comma-separated `namespace:key:type` pairs to preserve through reset |

### Build Partition Layout

`firmware/partitions.csv` exists for the build system only — the runtime ignores compiled-in offsets. `CONFIG_PARTITION_TABLE_OFFSET` in `sdkconfig.defaults` is a build-time artifact; runtime always scans.

## Code Style

- `safemode::` namespace, PascalCase classes, camelCase methods, `kConstantName` constants
- Allman brace style
- `static constexpr const char* kTag = "component_name"` for ESP_LOG tags
- No singletons — explicit construction wired in main.cpp
- `web_assets.h` / `web_assets.cmake` are generated — do not edit manually
