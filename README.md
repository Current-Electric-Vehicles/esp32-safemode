# esp32-safemode

A universal recovery partition for ESP32 devices. When your main app is bricked, safemode boots into a WiFi AP and serves a web UI for OTA-flashing new firmware. It works on any ESP32 device regardless of partition layout, supports flash encryption, and optionally provides factory reset with selective NVS key preservation.

## Why

ESP32 OTA updates can go wrong. A bad flash, a boot loop, or a corrupt app partition can leave your device unreachable. Safemode is a second app partition that's always there as a fallback — connect to its WiFi, open a browser, upload a new firmware binary, and you're back in business.

## How It Works

The ESP32 runs a two-partition OTA scheme. Your main app lives in one OTA slot, safemode lives in the other. When the device boots into safemode (via OTA data, watchdog reset, or manual trigger), it:

1. **Scans flash for the partition table** — doesn't assume any specific offset or layout
2. **Starts a WiFi AP** (SSID: `SAFEMODE`, password: `safemode`, IP: `4.3.2.1`)
3. **Serves a web UI** for uploading firmware, rebooting, and (optionally) factory reset
4. **Writes the uploaded binary** to the app partition and reboots into it

## Works With Any Partition Layout

Safemode doesn't hardcode partition offsets, sizes, or names. At startup it:

- Scans flash at 4KB-aligned offsets (0x4000–0x20000) for the partition table magic bytes
- Uses encrypted reads when flash encryption is enabled
- Registers all discovered partitions with ESP-IDF at runtime
- Finds the app partition dynamically (first OTA app partition not labeled "safemode")

This means you can drop the safemode binary into any ESP32 project's partition scheme. The only requirement is that your partition table has:
- An NVS partition (any size, any offset)
- An OTA data partition
- Two app partitions (one for your app, one for safemode)

The partition names, offsets, and sizes don't matter — safemode discovers everything at runtime.

## Flash Encryption Support

Safemode works on devices with flash encryption enabled:

- **Partition table scanning** uses `esp_flash_read_encrypted()` to read encrypted flash
- **OTA uploads** accept pre-encrypted `.enc.bin` files — written via `esp_partition_write_raw()` to avoid double-encryption
- **Non-encrypted devices** work too — plaintext `.bin` files are written via `esp_partition_write()` with transparent encryption handling

Upload whichever binary your build system produces — safemode detects the partition's encryption flag and uses the correct write path.

## Factory Reset

Safemode can optionally show a "Factory Reset" button in the web UI. This is controlled by your host app via NVS keys in the `"safemode"` namespace.

### Enabling Factory Reset

In your host app, write these NVS keys once (e.g., on first boot):

```cpp
#include "nvs.h"
#include "nvs_flash.h"

void configureSafemode()
{
    nvs_handle_t handle;
    if (nvs_open("safemode", NVS_READWRITE, &handle) != ESP_OK) return;

    // Enable the factory reset button
    nvs_set_u8(handle, "factoryResetEnabled", 1);

    // Keys to PRESERVE through a factory reset.
    // Format: "namespace:key:type" — comma-separated.
    // Everything NOT listed gets wiped.
    nvs_set_str(handle, "factoryResetPreserve",
        // Keep factory reset working after reset
        "safemode:factoryResetEnabled:u8,"
        "safemode:factoryResetPreserve:str,"
        // Your app's keys to keep
        "wifi:ssid:str,"
        "wifi:pass:str,"
        "device:serial:str"
    );

    nvs_commit(handle);
    nvs_close(handle);
}
```

### Preserve List Format

Each entry is `namespace:key:type` where type is one of:

| Type | NVS type | Description |
|------|----------|-------------|
| `str` | string | Null-terminated string (up to 4000 bytes) |
| `blob` | blob | Binary data (up to 4000 bytes) |
| `u8`, `i8` | uint8/int8 | 8-bit integer |
| `u16`, `i16` | uint16/int16 | 16-bit integer |
| `u32`, `i32` | uint32/int32 | 32-bit integer |
| `u64`, `i64` | uint64/int64 | 64-bit integer |

### How Factory Reset Works

1. Reads the preserve list from `safemode:factoryResetPreserve`
2. Backs up each listed key (with its typed value) into RAM
3. Calls `nvs_flash_erase()` — wipes **all** NVS data across all namespaces
4. Calls `nvs_flash_init()` — reinitializes the empty NVS partition
5. Writes back the preserved keys

NVS doesn't support selective deletion, so it's a full wipe-and-restore. Keys in the preserve list survive; everything else is gone.

If you include `safemode:factoryResetEnabled` and `safemode:factoryResetPreserve` in the preserve list, factory reset remains available after the reset. If you omit them, it's a one-shot reset — the button disappears on the next boot into safemode.

## Web UI

The frontend is a React SPA embedded directly in the firmware binary (gzip-compressed, ~30KB). It provides:

- **Firmware Update** — file picker, upload progress bar, auto-reboot on success
- **Leave Safemode** — sets boot partition to the app and reboots
- **Restart** — reboots back into safemode
- **Factory Reset** — (conditional) wipes NVS with key preservation, then offers reboot options
- **Device Info** — chip, cores, IDF version, free heap, partition info

## API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| POST | /api/ping | Health check |
| POST | /api/restart | Reboot (5s delay) |
| POST | /api/app | Set boot partition to app and reboot |
| POST | /api/update | Upload firmware binary, stream to flash |
| POST | /api/factory-reset | Wipe NVS with key preservation |
| GET | /api/info | Device metadata + factory reset status |
| OPTIONS | /api/* | CORS preflight |

## Building

Requires [ESP-IDF v6.0](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/get-started/) and Node.js 20+.

```sh
# Full build (frontend + firmware)
python scripts/build.py

# Firmware only (skip frontend rebuild)
python scripts/build.py --skip-frontend

# Release build
python scripts/build.py --release

# Flash to device
python scripts/flash.py

# Flash and monitor serial
python scripts/flash.py --monitor

# Clean everything
python scripts/clean.py --all
```

## Architecture

Pure ESP-IDF 6.0, C++20, `safemode::` namespace. No Arduino, no PlatformIO.

### Components

| Component | Purpose |
|-----------|---------|
| `components/partition_scan/` | Scans flash for partition table, registers partitions at runtime |
| `components/ota/` | Streaming OTA writer — handles both plaintext and pre-encrypted binaries |
| `components/factory_reset/` | NVS backup/erase/restore with typed key preservation |
| `components/wifi/` | WiFi AP, HTTP server, REST API, embedded web assets |

### Frontend

React 19 + Tailwind CSS v4 + Vite 8 + TypeScript. Located at `firmware/frontend/`. Build outputs are gzip-compressed and embedded into the firmware binary via `web_assets.h`.
