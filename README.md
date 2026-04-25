# esp32-safemode

A recovery partition for ESP32 devices. Boots into a WiFi AP and serves a web UI for OTA-flashing new application firmware — even when the main app is bricked.

## How It Works

The ESP32 runs a two-partition OTA scheme:

- **`app`** (ota_0) — the main application firmware
- **`safemode`** (ota_1) — this recovery firmware

When booted into safemode, the device:

1. Starts a WiFi access point (SSID: `SAFEMODE`, password: `safemode`)
2. Serves a web UI at `http://4.3.2.1`
3. Accepts firmware uploads (.bin) and writes them to the `app` partition
4. Reboots into the new firmware

Supports devices with **flash encryption enabled** — upload pre-encrypted `.enc.bin` files directly. The OTA updater detects encrypted partitions and writes raw ciphertext, bypassing the encryption layer to avoid double-encryption.

## Device Compatibility

Safemode is designed to run on arbitrary ESP32 devices without hardcoding partition layouts:

- **Partition table auto-discovery**: If the compiled-in partition table offset doesn't match the device, safemode scans flash (0x4000–0x20000) to find the table and registers partitions at runtime
- **Flash encryption aware**: Detects encryption and uses encrypted reads for partition table scanning
- **Dynamic target partition**: Finds the first app partition not labeled "safemode" — no hardcoded names or offsets
- **NVS resilience**: WiFi uses RAM-only storage; NVS is best-effort

## Building

Requires [ESP-IDF v6.0](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/get-started/) and Node.js 20+.

```sh
# Full build (frontend + firmware)
python scripts/build.py

# Firmware only (skip frontend)
python scripts/build.py --skip-frontend

# Release build
python scripts/build.py --release

# Flash to device
python scripts/flash.py

# Flash and open serial monitor
python scripts/flash.py --monitor

# Clean everything
python scripts/clean.py --all
```

Frontend dev server (from `firmware/frontend/`):

```sh
npm run dev      # Vite dev server, proxies /api to 4.3.2.1
npm run build    # TypeScript check + Vite production build
```

## Partition Layout

Configured in `firmware/partitions.csv`. Partition table is at offset `0x10000`.

| Name     | Type | SubType | Offset   | Size   | Flags     |
|----------|------|---------|----------|--------|-----------|
| nvs      | data | nvs     | 0x11000  | 52KB   |           |
| otadata  | data | ota     | 0x1E000  | 8KB    |           |
| app      | app  | ota_0   | 0x20000  | 3.06MB | encrypted |
| safemode | app  | ota_1   | 0x330000 | 896KB  | encrypted |
| spiffs   | data | spiffs  | 0x410000 | 3.9MB  |           |

## Architecture

Pure ESP-IDF 6.0, C++20, `safemode::` namespace. No Arduino, no PlatformIO.

### Components

- **`components/ota/`** — `OtaUpdater`: streaming OTA with separate paths for plaintext and pre-encrypted binaries
- **`components/wifi/`** — `WifiAp` (AP mode, RAM-only storage), `HttpServer` (REST API + embedded web assets)
- **`components/partition_scan/`** — runtime partition table discovery for cross-device compatibility

### API Endpoints

| Method  | Path          | Description                                  |
|---------|---------------|----------------------------------------------|
| POST    | /api/ping     | Health check                                 |
| POST    | /api/restart  | Schedule reboot (5s delay)                   |
| POST    | /api/app      | Set boot partition to "app" and reboot       |
| POST    | /api/update   | Upload firmware binary, stream to OTA writer |
| GET     | /api/info     | Device metadata (chip, heap, partitions)     |
| OPTIONS | /api/*        | CORS preflight                               |

### Frontend

React 19 + Tailwind CSS v4 + Vite 8 + TypeScript. Located at `firmware/frontend/`. Build outputs are gzip-compressed and embedded into the firmware binary via `web_assets.h`.
