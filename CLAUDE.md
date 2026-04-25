# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 "safemode" firmware — a recovery partition that lets users OTA-flash new application firmware over WiFi. The device exposes an AP ("SAFEMODE") and serves a React-based upload UI.

Two-partition boot scheme: `app` (ota_0) is the main application; `safemode` (ota_1) is the always-present recovery image. The safemode firmware writes new binaries to the `app` partition and reboots into it.

Designed to run on arbitrary ESP32 devices — auto-discovers the partition table at runtime and supports flash encryption (pre-encrypted `.enc.bin` uploads).

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

### Components

- **`components/ota/`** — `OtaUpdater` class: streaming OTA with separate paths for plaintext (`esp_ota_write`) and pre-encrypted (`esp_partition_write_raw`) binaries. Finds first app partition not labeled "safemode".
- **`components/wifi/`** — WiFi AP (`WifiAp`, RAM-only storage), HTTP server (`HttpServer`) with REST API + embedded web assets.
- **`components/partition_scan/`** — Runtime partition table discovery. Scans flash for the partition table if the compiled-in offset doesn't match, handles encrypted flash reads, registers found partitions via `esp_partition_register_external()`.

### main/main.cpp

Wires everything: NVS init (with partition scan fallback), WiFi AP start (SSID "SAFEMODE", pass "safemode", IP 4.3.2.1), OTA updater + HTTP server start, idle loop.

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

Partition table at offset `0x10000`. Configured in `firmware/partitions.csv`.

| Name     | Type | SubType | Offset   | Size   | Flags     |
|----------|------|---------|----------|--------|-----------|
| nvs      | data | nvs     | 0x11000  | 52KB   |           |
| otadata  | data | ota     | 0x1E000  | 8KB    |           |
| app      | app  | ota_0   | 0x20000  | 3.06MB | encrypted |
| safemode | app  | ota_1   | 0x330000 | 896KB  | encrypted |
| spiffs   | data | spiffs  | 0x410000 | 3.9MB  |           |

## Code Style

- `safemode::` namespace, PascalCase classes, camelCase methods, `kConstantName` constants
- Allman brace style
- `static constexpr const char* kTag = "component_name"` for ESP_LOG tags
- No singletons — explicit construction wired in main.cpp
- `web_assets.h` / `web_assets.cmake` are generated — do not edit manually
