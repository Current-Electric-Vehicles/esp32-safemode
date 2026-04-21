# ESP32 Safemode: ESP-IDF Refactor Design

Complete rewrite of the safemode firmware from PlatformIO/Arduino to pure ESP-IDF 6.0 with a component-based architecture, Python build scripts, and a modern React/Tailwind frontend.

## Goals

- Remove PlatformIO and all Arduino dependencies
- Pure ESP-IDF 6.0, C++20, `safemode::` namespace
- Modular ESP-IDF component architecture (matching current-ledcontroller and current-hcu patterns)
- Python build scripts (adapted from HCU, no encryption)
- Modern frontend: React 19 + Tailwind CSS v4 + Vite 8 (matching HCU stack)
- Functional parity with current firmware: OTA update, boot-into-app, restart, captive portal

## Hardware

- ESP32-WROOM-32E-N8R2 (dual-core Xtensa LX6 @ 240MHz, 8MB flash, 2MB PSRAM)
- IDF target: `esp32`
- Same board as the GlowCraft ledcontroller project

## Repository Structure

```
esp32-safemode/
├── scripts/                     # Python build tooling
│   ├── common.py                # IDF discovery, env caching, serial port detection
│   ├── build.py                 # Build firmware (+ frontend if present)
│   ├── build_frontend.py        # Frontend build + web_assets.h generation
│   ├── flash.py                 # Flash to device (full or app-only)
│   ├── clean.py                 # Remove build artifacts
│   ├── monitor.py               # Serial monitor wrapper
│   ├── menuconfig.py            # IDF menuconfig wrapper
│   └── idf.py                   # Drop-in idf.py proxy
├── firmware/                    # ESP-IDF project root
│   ├── CMakeLists.txt           # Minimal: cmake_minimum_required + include project.cmake
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   └── main.cpp             # app_main() entry point
│   ├── components/
│   │   ├── wifi/                # WiFi AP + HTTP server + DNS + web assets
│   │   └── ota/                 # OTA update logic
│   ├── frontend/                # React/Tailwind SPA
│   ├── partitions.csv
│   ├── sdkconfig.defaults
│   └── sdkconfig.defaults.release
├── CLAUDE.md
└── docs/
```

### What Gets Deleted

- `firmware/platformio.ini`
- `firmware/build_web.py`
- `firmware/src/` (main.cpp, SafemodeWebServer.cpp)
- `firmware/include/` (SafemodeWebServer.h, StaticFiles.h)
- `firmware/CMakeLists.txt` (replaced with new minimal version)
- `firmware/sdkconfig` and `firmware/sdkconfig.local`
- `web/` directory (moves to `firmware/frontend/`)
- All Arduino library references (WiFi.h, DNSServer.h, aWOT.h)

## Components

### `components/ota/`

Isolates OTA update logic from the HTTP transport.

```
ota/
├── CMakeLists.txt          # REQUIRES esp_ota_ops esp_partition
├── include/
│   └── ota_updater.h       # OtaUpdater class
└── src/
    └── ota_updater.cpp
```

**OtaUpdater class** — streaming OTA interface:
- `esp_err_t begin()` — finds "app" partition, calls `esp_ota_begin()`
- `esp_err_t write(const void* data, size_t len)` — calls `esp_ota_write()`
- `esp_err_t finish()` — calls `esp_ota_end()`, sets boot partition
- `void abort()` — calls `esp_ota_abort()` for error cleanup

Replaces the current raw `esp_flash_read/erase/write` sector-by-sector approach with the proper ESP-IDF OTA API, which is safer and handles flash encryption transparently.

### `components/wifi/`

Handles WiFi AP, HTTP server, DNS captive portal, and embedded web assets.

```
wifi/
├── CMakeLists.txt          # EMBED_FILES from web_assets.cmake
├── include/
│   ├── http_server.h
│   ├── wifi_ap.h
│   └── dns_server.h
├── src/
│   ├── http_server.cpp
│   ├── wifi_ap.cpp
│   ├── dns_server.cpp
│   └── web_assets.h        # Auto-generated
├── web/                    # Auto-generated: gzipped dist files
└── web_assets.cmake        # Auto-generated: EMBED_FILES list
```

**WifiAp** (static class):
- `start()` — creates AP with SSID "SAFEMODE", password "safemode", IP `4.3.2.1`, subnet `255.255.255.0`
- Uses `esp_netif` + `esp_wifi` APIs (no Arduino WiFi.h)

**DNS Server** (free functions):
- `dnsServerStart()` — spawns FreeRTOS task, UDP socket on port 53
- `dnsServerStop()` — signals task to exit, cleans up
- All DNS A-record queries resolve to `4.3.2.1` (captive portal)
- Adapted from HCU's dns_server.cpp with IP changed to `4.3.2.1`

**HttpServer** class:
- Uses `esp_http_server` (native ESP-IDF HTTP server)
- Wildcard URI matching for SPA fallback
- CORS headers on all `/api/*` routes

API routes (all return JSON `{"result": true/false}`):
- `POST /api/ping` — health check
- `POST /api/restart` — schedules reboot via `esp_restart()` after 5-second delay
- `POST /api/app` — sets boot partition to "app", schedules reboot
- `POST /api/update` — receives firmware binary in request body, streams to OtaUpdater
- `GET /api/info` — returns device metadata (chip info, partition info, free heap, IDF version)
- `OPTIONS /api/*` — CORS preflight

Static file serving:
- Registers each `kWebAssets[]` entry as a GET route
- Sets `Content-Encoding: gzip` for compressed assets
- Sets `Cache-Control: public, max-age=31536000, immutable` for hashed `.js`/`.css` files
- SPA fallback: unknown GET routes serve `index.html`

### `main/main.cpp`

Wires everything together:
1. Initialize NVS (with erase-on-corruption fallback)
2. Start WiFi AP
3. Start DNS server
4. Create OtaUpdater instance
5. Start HTTP server (with OtaUpdater wired in)
6. Enter idle loop (`vTaskDelay`)

No settings/NVS configuration needed — SSID, password, and IP are hardcoded constants (this is a recovery partition, not a configurable application).

## Frontend

### Stack
- React 19, react-dom 19
- Tailwind CSS v4 with `@tailwindcss/vite`
- Vite 8, `@vitejs/plugin-react`
- TypeScript 6
- No react-router (single page)
- No external component libraries

### UI Layout

Single page with three sections:

**Status Header:**
- "SAFEMODE" title
- Connectivity indicator (polls `POST /api/ping` every 5 seconds)

**OTA Update Section:**
- File input for `.bin` firmware file
- Upload button (disabled until file selected)
- Progress bar during upload (uses XMLHttpRequest for progress events)
- Success/error message display

**Actions Section:**
- "Leave Safemode" button — calls `POST /api/app` with confirmation dialog
- "Restart" button — calls `POST /api/restart`

**Device Info Section** (collapsible or footer):
- Chip model, revision
- IDF version
- Free heap
- App partition status
- Data from `GET /api/info`

### Styling
- Dark theme: dark background, stone neutrals, orange accent for primary actions
- Mobile-first (captive portal is typically accessed from phones)
- Utility-first Tailwind classes, no separate CSS files

### API Layer (`api.ts`)
```typescript
ping(): Promise<boolean>
restart(): Promise<void>
bootIntoApp(): Promise<void>
uploadFirmware(file: File, onProgress?: (pct: number) => void): Promise<void>
getInfo(): Promise<DeviceInfo>
```

`uploadFirmware` uses `XMLHttpRequest` for upload progress events. All others use `fetch`.

Dev mode: Vite proxy points to `http://4.3.2.1:80` for API calls during development.

## Build System

### `scripts/common.py`
Adapted from HCU. Key constants:
- `DEFAULT_TARGET = "esp32"`
- `ADDR_BOOTLOADER = "0x1000"` (ESP32, not S3's `0x0`)
- `ADDR_PARTITION_TABLE = "0x8000"`
- Log prefix: `[safemode]`

Functions kept: `_find_idf_path()`, `_get_idf_env()` (with caching), `get_idf_env()`, `run_idf()`, `_find_idf_python()`, `_get_idf_python()`, `detect_serial_port()`, `resolve_port()`, `parse_partitions_csv()`, `add_common_args()`, `add_port_args()`, `get_build_dir()`, `get_sdkconfig_defaults()`.

Functions removed (encryption): `encrypt_flash_data()`, `run_espsecure()`, `run_esptool()`, `run_espefuse()`, `get_idf_python()` (public), `_find_esptool()`, `FLASH_ENC_KEY`.

IDF search paths: `~/esp/esp-idf`, `~/esp/v6.0/esp-idf`, `~/.espressif/esp-idf`, `/opt/esp-idf`.

### `scripts/build.py`
```
python scripts/build.py [--release] [--skip-frontend] [-v]
```
1. Build frontend if `firmware/frontend/` exists and not `--skip-frontend`
2. Call `run_idf("build", ...)`

No encryption step. No firmware version generation.

### `scripts/build_frontend.py`
Copied from HCU — identical pipeline:
1. Detect Node.js (nvm or PATH, with setup instructions on failure)
2. `npm install` if `node_modules/` missing
3. `npm run build`
4. Copy `dist/` to `firmware/components/wifi/web/`, gzip text assets (level 9)
5. Generate `web_assets.h` with `_binary_*_start/_end` externs and `kWebAssets[]` table
6. Generate `web_assets.cmake` with `set(WEB_EMBED_FILES ...)` for CMake

### `scripts/flash.py`
```
python scripts/flash.py [--release] [--app-only] [-p PORT] [-b BAUD] [--monitor]
```
Uses `idf.py flash` or `idf.py app-flash`. No encrypted flashing paths.

### `scripts/clean.py`, `scripts/monitor.py`, `scripts/menuconfig.py`, `scripts/idf.py`
Direct adaptations from HCU with `[safemode]` log prefix.

### sdkconfig.defaults
```
CONFIG_IDF_TARGET="esp32"
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
# CONFIG_COMPILER_CXX_EXCEPTIONS is not set
# CONFIG_COMPILER_CXX_RTTI is not set
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10
CONFIG_ESP_SYSTEM_PANIC_PRINT_HALT=y
CONFIG_HTTPD_MAX_URI_HANDLERS=16
CONFIG_LWIP_MAX_SOCKETS=8
```

### Partition Layout (unchanged)
```
nvs,        data,   nvs,        0x11000,    0xD000,
otadata,    data,   ota,        0x1E000,    0x2000,
safemode,   app,    ota_0,      0x20000,    0x1B0000,
app,        app,    ota_1,      0x1D0000,   0x1B0000,
spiffs,     data,   spiffs,     0x380000,   0x7A120,
```

## Code Style

- C++20, pure ESP-IDF (no Arduino, no PlatformIO)
- `safemode::` namespace for all custom code
- PascalCase classes, camelCase methods, `kConstantName` constants
- No singletons — explicit construction, wired via references in main.cpp
- Allman brace style (matching HCU/ledcontroller)
- `static constexpr const char* kTag = "component_name"` for ESP_LOG tags

## Refactor Loop Plan

Iterative agent-driven refactor following the ledcontroller's proven loop structure.

### Loop Structure
```
ITERATION N:
  Step 1: PLAN        — Opus agent audits current state vs target, writes .claude/refactor-plan.md
  Step 1b: VERIFY     — (only on REFACTOR_COMPLETE) Fresh Opus agent independently verifies
  Step 2: EXECUTE     — Opus agent implements the plan
  Step 3: BUILD       — python scripts/build.py, fix if needed
  Step 4: COMMIT      — Auto-commit
  Step 5: UPDATE DOCS — Update CLAUDE.md, .claude/memory/ if needed
```

### Completion Checklist

**Infrastructure:**
- [ ] PlatformIO files removed
- [ ] Arduino dependencies removed
- [ ] Python scripts/ created and functional
- [ ] Top-level CMakeLists.txt + main/CMakeLists.txt created
- [ ] sdkconfig.defaults created
- [ ] partitions.csv preserved

**Components:**
- [ ] `wifi` component: WiFi AP (SSID "SAFEMODE", pass "safemode", IP 4.3.2.1)
- [ ] `wifi` component: DNS captive portal (all queries -> 4.3.2.1)
- [ ] `wifi` component: HTTP server with static file serving + SPA fallback
- [ ] `wifi` component: API routes (ping, restart, app, update, info)
- [ ] `wifi` component: CORS middleware
- [ ] `wifi` component: web_assets.h / web_assets.cmake integration
- [ ] `ota` component: OtaUpdater with begin/write/finish/abort
- [ ] `main/main.cpp`: wires everything together

**Frontend:**
- [ ] React 19 + Tailwind v4 + Vite 8 project in firmware/frontend/
- [ ] OTA upload with progress feedback
- [ ] Leave Safemode button with confirmation
- [ ] Restart button
- [ ] Connectivity indicator
- [ ] Device info section (GET /api/info)
- [ ] Builds and embeds correctly via build_frontend.py

**Integration:**
- [ ] `python scripts/build.py` succeeds
- [ ] Firmware binary fits in safemode partition (1.7MB)
- [ ] Old Arduino/PlatformIO code fully removed

### Completion Criteria
1. Planner declares REFACTOR_COMPLETE (all checklist items done)
2. Final Verification agent independently confirms REFACTOR_TRULY_COMPLETE
3. Functional parity: OTA upload, boot-into-app, restart, captive portal, device info
4. Firmware builds successfully and fits in partition
5. No TODO stubs remaining
