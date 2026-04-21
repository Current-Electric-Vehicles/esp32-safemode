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
- `web_assets.h` / `web_assets.cmake` are generated — do not edit manually
