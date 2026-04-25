# Universal Safemode with Factory Reset

**Date**: 2026-04-24

## Goal

Make safemode firmware truly partition-table-agnostic (works on any ESP32 device without recompilation) and add NVS-based factory reset with selective key preservation.

## 1. Partition-Agnostic Runtime

### Current Problem

The firmware has `CONFIG_PARTITION_TABLE_OFFSET=0x10000` hardcoded and tries native ESP-IDF partition loading first, falling back to the scanner only on failure. This means the compiled-in offset must match the target device or the fallback path is taken — which doesn't work correctly with encrypted OTA (externally-registered partitions lack proper encryption integration).

### Design

Always scan flash for the partition table at startup. Remove the "try compiled-in offset, then fall back" logic.

**Startup sequence:**

1. Scan flash at 4KB-aligned offsets from 0x4000 to 0x20000 for partition table magic (0x50AA)
2. Use `esp_flash_read_encrypted()` when flash encryption is detected
3. Parse all partition entries and register them via `esp_partition_register_external()`
4. Init NVS from the discovered NVS partition — handle `NO_FREE_PAGES` / `NEW_VERSION_FOUND` with erase+retry
5. If NVS init fails entirely, continue without it (WiFi uses RAM-only storage)
6. Start WiFi AP, HTTP server, OTA updater

**Build artifacts**: `partitions.csv` and `CONFIG_PARTITION_TABLE_OFFSET` remain for the build system. The runtime ignores the compiled-in offset entirely.

**Impact on OTA**: Since partitions are always externally registered, `esp_ota_begin/write/end` may not work correctly — their validation reads use `bootloader_flash_read()` which doesn't integrate with external partitions. We must **drop the ESP-IDF OTA APIs and manage writes directly**.

### OTA Updater Changes

Replace `esp_ota_begin/write/end` with direct partition operations:

- Erase with `esp_partition_erase_range()`
- Write with `esp_partition_write_raw()` for encrypted partitions (pre-encrypted `.enc.bin`)
- Write with `esp_partition_write()` for non-encrypted partitions (plaintext `.bin`, encryption handled transparently by partition API)
- No image validation step (the old `esp_ota_end()` can't work with external partitions)
- Set boot target with `esp_ota_set_boot_partition()` (works with external partitions)

## 2. Factory Reset via NVS

### NVS Convention

The host application writes configuration into the `"safemode"` NVS namespace:

| Key | Type | Description |
|-----|------|-------------|
| `factoryResetEnabled` | `uint8_t` | Set to `1` to enable factory reset in the UI |
| `factoryResetPreserve` | `string` (max 4000 bytes) | Comma-separated `namespace:key` pairs to preserve through reset |

Example preserve list: `wifi:ssid,wifi:pass,device:serial,safemode:factoryResetEnabled,safemode:factoryResetPreserve`

Note: The host app should include `safemode:factoryResetEnabled` and `safemode:factoryResetPreserve` in the preserve list if they want factory reset to remain enabled after a reset.

### Factory Reset Flow

1. Firmware reads `factoryResetEnabled` from NVS namespace `"safemode"` at startup (or when `/api/info` is called)
2. If enabled, the UI shows a "Factory Reset" button in the Actions section
3. User clicks the button, sees a confirmation dialog
4. On confirm, `POST /api/factory-reset` is called
5. Firmware:
   a. Reads `factoryResetPreserve` from NVS namespace `"safemode"`
   b. Parses the comma-separated `namespace:key` pairs
   c. Reads and backs up each listed key's value (type + data) into RAM
   d. Calls `nvs_flash_erase()` to wipe all NVS data
   e. Calls `nvs_flash_init()` to reinitialize
   f. Writes back all preserved key/value pairs
   g. Returns success JSON
6. UI shows success message with two buttons: "Reboot into App" and "Stay in Safemode"

### Error Handling

- If `factoryResetPreserve` key doesn't exist: wipe everything, no keys preserved
- If a preserved key doesn't exist at read time: skip it silently
- If NVS erase fails: return error, don't attempt restore
- If restore of a preserved key fails: log warning, continue with remaining keys

## 3. API Changes

### New Endpoint

**`POST /api/factory-reset`**

Wipes NVS, preserving keys listed in `safemode:factoryResetPreserve`.

Response (success):
```json
{"result": true}
```

Response (error — factory reset not enabled or NVS failure):
```json
{"result": false}
```

### Modified Endpoint

**`GET /api/info`** — add `factoryResetEnabled` field:

```json
{
  "chip": "ESP32",
  "cores": 2,
  "idfVersion": "v6.0",
  "freeHeap": 180000,
  "runningPartition": "safemode",
  "appPartition": "app",
  "firmwareVersion": "abc123",
  "factoryResetEnabled": true
}
```

## 4. Frontend Changes

### DeviceInfo Interface

Add `factoryResetEnabled: boolean` to the `DeviceInfo` TypeScript interface.

### API Client

Add `factoryReset(): Promise<boolean>` function that POSTs to `/api/factory-reset`.

### UI Changes

In the Actions section of `App.tsx`:

- If `deviceInfo.factoryResetEnabled` is true, render a "Factory Reset" button (styled distinctly — red/destructive)
- On click: `window.confirm("This will erase all device settings. Continue?")` 
- On confirm: call `factoryReset()`, show success/error message
- On success: show two buttons — "Reboot into App" and "Stay in Safemode" (reuses existing `restart()` and `bootIntoApp()` calls)

## 5. Files Changed

### Firmware

| File | Change |
|------|--------|
| `main/main.cpp` | Always scan first, remove try-native-first logic. Read factory reset config from NVS. |
| `components/partition_scan/src/partition_scan.cpp` | No changes needed — already scans correctly |
| `components/ota/src/ota_updater.cpp` | Unify on raw-write path: erase + `esp_partition_write_raw` (encrypted) or `esp_partition_write` (plaintext). Drop `esp_ota_begin/write/end`. |
| `components/ota/include/ota_updater.h` | Remove OTA handle, simplify interface |
| `components/wifi/src/http_server.cpp` | Add `/api/factory-reset` handler, add `factoryResetEnabled` to info response |
| `components/wifi/include/http_server.h` | Add factory reset state/method |

### Frontend

| File | Change |
|------|--------|
| `firmware/frontend/src/api.ts` | Add `factoryReset()`, update `DeviceInfo` interface |
| `firmware/frontend/src/App.tsx` | Conditional factory reset button, post-reset reboot options |

### Config

| File | Change |
|------|--------|
| `firmware/sdkconfig.defaults` | Keep `CONFIG_PARTITION_TABLE_OFFSET` for the build system — no runtime changes needed, runtime already ignores it |

## 6. NVS Key Backup Implementation Detail

NVS stores values with types (uint8, int32, string, blob, etc.). The backup must preserve the type.

### Preserve List Format

Each entry is `namespace:key:type` where type is required:

- `str` — string
- `blob` — binary blob
- `u8`, `i8`, `u16`, `i16`, `u32`, `i32`, `u64`, `i64` — integers

Example: `wifi:ssid:str,wifi:pass:str,device:serial:str,safemode:factoryResetEnabled:u8,safemode:factoryResetPreserve:str`

### Backup Structure

```
struct PreservedKey {
    char ns[16];
    char key[16];
    nvs_type_t type;
    size_t dataLen;
    uint8_t data[4000];
};
```

Memory: each entry is ~4KB. The preserve list is parsed and backed up sequentially — only one `PreservedKey` needs to be in memory at a time (read, store in a vector, repeat). With typical preserve lists of 5–15 keys, total RAM usage is 20–60KB, well within ESP32 DRAM/PSRAM limits.

### Restore Sequence

1. Parse preserve list into `namespace:key:type` triples
2. For each triple: open namespace, read value with the typed getter, push to vector
3. `nvs_flash_erase()` — wipe all NVS
4. `nvs_flash_init()` — reinitialize
5. For each backed-up entry: open namespace, write with typed setter, commit
