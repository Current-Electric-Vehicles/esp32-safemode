# Universal Safemode + Factory Reset Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make safemode firmware fully partition-table-agnostic at runtime and add NVS factory reset with selective key preservation.

**Architecture:** Always scan flash for partitions at startup (no compiled-in offset dependency). Drop ESP-IDF OTA APIs in favor of direct partition read/write. New `factory_reset` component handles NVS backup/erase/restore. Frontend conditionally shows factory reset button based on NVS config.

**Tech Stack:** ESP-IDF 6.0, C++20, React 19, TypeScript, Tailwind CSS v4

---

### Task 1: Rewrite main.cpp for Scan-First Startup

**Files:**
- Modify: `firmware/main/main.cpp`

- [ ] **Step 1: Rewrite main.cpp**

Replace the current "try native, then scan" logic with always-scan-first. NVS init becomes best-effort (continue without it for WiFi RAM-only storage).

```cpp
#include "esp_log.h"
#include "http_server.h"
#include "nvs_flash.h"
#include "ota_updater.h"
#include "partition_scan.h"
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

    // Always scan flash for partition table — safemode is partition-agnostic.
    esp_err_t ret = safemode::scanAndRegisterPartitions();
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to discover partitions — cannot continue");
        return;
    }

    // Initialize NVS (best-effort — WiFi uses RAM-only storage as fallback)
    bool nvsAvailable = false;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(kTag, "NVS partition needs erase...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret == ESP_OK)
    {
        nvsAvailable = true;
    }
    else
    {
        ESP_LOGW(kTag, "NVS unavailable (%s), continuing without it", esp_err_to_name(ret));
    }

    // Start WiFi AP
    ret = safemode::WifiAp::start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to start WiFi AP: %s", esp_err_to_name(ret));
        return;
    }

    // Create OTA updater
    safemode::OtaUpdater otaUpdater;

    // Start HTTP server
    safemode::HttpServer httpServer;
    httpServer.setOtaUpdater(&otaUpdater);
    httpServer.setNvsAvailable(nvsAvailable);

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

- [ ] **Step 2: Build to verify compilation**

Run: `python scripts/build.py --skip-frontend`
Expected: Compilation error for `setNvsAvailable` (not yet added to HttpServer). This confirms main.cpp changes are syntactically correct and the partition_scan linkage works. We'll fix the HttpServer in Task 4.

- [ ] **Step 3: Commit**

```bash
git add firmware/main/main.cpp
git commit -m "refactor: always scan flash for partitions at startup"
```

---

### Task 2: Rewrite OTA Updater — Drop ESP-IDF OTA APIs

**Files:**
- Modify: `firmware/components/ota/include/ota_updater.h`
- Modify: `firmware/components/ota/src/ota_updater.cpp`
- Modify: `firmware/components/ota/CMakeLists.txt`

- [ ] **Step 1: Rewrite ota_updater.h**

Remove `esp_ota_handle_t`, remove `esp_ota_ops.h` include. The class now manages partition operations directly.

```cpp
#pragma once

#include "esp_err.h"
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

    /// Set the target partition for OTA updates. If not set, begin() will
    /// search for the first app partition not labeled "safemode".
    void setTargetPartition(const esp_partition_t* partition) { targetPartition_ = partition; }

    /// Begin an OTA update. Erases the target partition.
    esp_err_t begin();

    /// Write a chunk of firmware data.
    esp_err_t write(const void* data, size_t len);

    /// Finish the update. Sets the boot partition.
    esp_err_t finish();

    /// Abort an in-progress update.
    void abort();

    /// Returns true if an update is in progress.
    bool isActive() const { return active_; }

private:
    static constexpr const char* kTag = "ota";

    const esp_partition_t* targetPartition_ = nullptr;
    const esp_partition_t* partition_ = nullptr;
    bool active_ = false;
    bool encrypted_ = false;
    size_t writeOffset_ = 0;
};

}  // namespace safemode
```

- [ ] **Step 2: Rewrite ota_updater.cpp**

Replace all `esp_ota_begin/write/end` with direct partition operations. Encrypted partitions use `esp_partition_write_raw()`, non-encrypted use `esp_partition_write()`.

```cpp
#include "ota_updater.h"

#include "esp_log.h"
#include "esp_ota_ops.h"

#include <cstring>

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

    if (targetPartition_ != nullptr)
    {
        partition_ = targetPartition_;
    }
    else
    {
        // Find the first app partition not labeled "safemode"
        esp_partition_iterator_t it = esp_partition_find(
            ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
        while (it != nullptr)
        {
            const esp_partition_t* p = esp_partition_get(it);
            if (strcmp(p->label, "safemode") != 0)
            {
                partition_ = p;
                break;
            }
            it = esp_partition_next(it);
        }
        if (it != nullptr)
        {
            esp_partition_iterator_release(it);
        }
    }

    if (partition_ == nullptr)
    {
        ESP_LOGE(kTag, "No target app partition found");
        return ESP_ERR_NOT_FOUND;
    }

    encrypted_ = partition_->encrypted;
    ESP_LOGI(kTag, "Starting OTA update to partition '%s' @ 0x%lx (%lu bytes, encrypted=%d)",
             partition_->label, partition_->address, partition_->size, encrypted_);

    esp_err_t ret = esp_partition_erase_range(partition_, 0, partition_->size);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Partition erase failed: %s", esp_err_to_name(ret));
        partition_ = nullptr;
        return ret;
    }

    active_ = true;
    writeOffset_ = 0;
    return ESP_OK;
}

esp_err_t OtaUpdater::write(const void* data, size_t len)
{
    if (!active_)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret;
    if (encrypted_)
    {
        ret = esp_partition_write_raw(partition_, writeOffset_, data, len);
    }
    else
    {
        ret = esp_partition_write(partition_, writeOffset_, data, len);
    }

    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "OTA write failed at offset %u: %s", (unsigned)writeOffset_, esp_err_to_name(ret));
        abort();
        return ret;
    }
    writeOffset_ += len;
    return ESP_OK;
}

esp_err_t OtaUpdater::finish()
{
    if (!active_)
    {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(kTag, "OTA: wrote %u bytes, setting boot partition...", (unsigned)writeOffset_);

    esp_err_t ret = esp_ota_set_boot_partition(partition_);
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
        ESP_LOGW(kTag, "OTA update aborted");
    }
    active_ = false;
    partition_ = nullptr;
    encrypted_ = false;
    writeOffset_ = 0;
}

}  // namespace safemode
```

- [ ] **Step 3: Update CMakeLists.txt**

Remove `spi_flash` from REQUIRES (no longer needed — we don't use `esp_flash_read` directly). Keep `app_update` for `esp_ota_set_boot_partition`.

```cmake
idf_component_register(
    SRCS
        "src/ota_updater.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        app_update
        esp_partition
)
```

- [ ] **Step 4: Build to verify**

Run: `python scripts/build.py --skip-frontend`
Expected: Build fails due to missing `setNvsAvailable` in HttpServer. OTA component itself should compile clean.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/ota/
git commit -m "refactor: drop esp_ota APIs, use direct partition operations"
```

---

### Task 3: Create Factory Reset Component

**Files:**
- Create: `firmware/components/factory_reset/CMakeLists.txt`
- Create: `firmware/components/factory_reset/include/factory_reset.h`
- Create: `firmware/components/factory_reset/src/factory_reset.cpp`

- [ ] **Step 1: Create CMakeLists.txt**

```cmake
idf_component_register(
    SRCS
        "src/factory_reset.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        nvs_flash
)
```

- [ ] **Step 2: Create factory_reset.h**

```cpp
#pragma once

#include "esp_err.h"

namespace safemode
{

/// Check if factory reset is enabled in the "safemode" NVS namespace.
/// Returns true if the "factoryResetEnabled" key is set to 1.
/// Returns false if NVS is unavailable or the key is missing/zero.
bool isFactoryResetEnabled();

/// Perform factory reset: read the preserve list from NVS, back up
/// listed keys, erase NVS, restore preserved keys.
/// Returns ESP_OK on success, error code on failure.
esp_err_t performFactoryReset();

}  // namespace safemode
```

- [ ] **Step 3: Create factory_reset.cpp**

```cpp
#include "factory_reset.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <cstring>
#include <vector>

namespace safemode
{

static constexpr const char* kTag = "factory_reset";
static constexpr const char* kNamespace = "safemode";
static constexpr const char* kEnabledKey = "factoryResetEnabled";
static constexpr const char* kPreserveKey = "factoryResetPreserve";
static constexpr size_t kMaxValueSize = 4000;

struct PreservedEntry
{
    char ns[16];
    char key[16];
    nvs_type_t type;
    size_t dataLen;
    uint8_t data[kMaxValueSize];
};

static nvs_type_t parseType(const char* typeStr)
{
    if (strcmp(typeStr, "str") == 0)  return NVS_TYPE_STR;
    if (strcmp(typeStr, "blob") == 0) return NVS_TYPE_BLOB;
    if (strcmp(typeStr, "u8") == 0)   return NVS_TYPE_U8;
    if (strcmp(typeStr, "i8") == 0)   return NVS_TYPE_I8;
    if (strcmp(typeStr, "u16") == 0)  return NVS_TYPE_U16;
    if (strcmp(typeStr, "i16") == 0)  return NVS_TYPE_I16;
    if (strcmp(typeStr, "u32") == 0)  return NVS_TYPE_U32;
    if (strcmp(typeStr, "i32") == 0)  return NVS_TYPE_I32;
    if (strcmp(typeStr, "u64") == 0)  return NVS_TYPE_U64;
    if (strcmp(typeStr, "i64") == 0)  return NVS_TYPE_I64;
    return NVS_TYPE_BLOB;  // default fallback
}

static esp_err_t readEntry(PreservedEntry& entry)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(entry.ns, NVS_READONLY, &handle);
    if (ret != ESP_OK)
    {
        ESP_LOGW(kTag, "Cannot open namespace '%s': %s", entry.ns, esp_err_to_name(ret));
        return ret;
    }

    entry.dataLen = kMaxValueSize;

    switch (entry.type)
    {
        case NVS_TYPE_U8:   entry.dataLen = 1; ret = nvs_get_u8(handle, entry.key, reinterpret_cast<uint8_t*>(entry.data)); break;
        case NVS_TYPE_I8:   entry.dataLen = 1; ret = nvs_get_i8(handle, entry.key, reinterpret_cast<int8_t*>(entry.data)); break;
        case NVS_TYPE_U16:  entry.dataLen = 2; ret = nvs_get_u16(handle, entry.key, reinterpret_cast<uint16_t*>(entry.data)); break;
        case NVS_TYPE_I16:  entry.dataLen = 2; ret = nvs_get_i16(handle, entry.key, reinterpret_cast<int16_t*>(entry.data)); break;
        case NVS_TYPE_U32:  entry.dataLen = 4; ret = nvs_get_u32(handle, entry.key, reinterpret_cast<uint32_t*>(entry.data)); break;
        case NVS_TYPE_I32:  entry.dataLen = 4; ret = nvs_get_i32(handle, entry.key, reinterpret_cast<int32_t*>(entry.data)); break;
        case NVS_TYPE_U64:  entry.dataLen = 8; ret = nvs_get_u64(handle, entry.key, reinterpret_cast<uint64_t*>(entry.data)); break;
        case NVS_TYPE_I64:  entry.dataLen = 8; ret = nvs_get_i64(handle, entry.key, reinterpret_cast<int64_t*>(entry.data)); break;
        case NVS_TYPE_STR:  ret = nvs_get_str(handle, entry.key, reinterpret_cast<char*>(entry.data), &entry.dataLen); break;
        case NVS_TYPE_BLOB: ret = nvs_get_blob(handle, entry.key, entry.data, &entry.dataLen); break;
        default:            ret = ESP_ERR_INVALID_ARG; break;
    }

    nvs_close(handle);
    return ret;
}

static esp_err_t writeEntry(const PreservedEntry& entry)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(entry.ns, NVS_READWRITE, &handle);
    if (ret != ESP_OK)
    {
        ESP_LOGW(kTag, "Cannot open namespace '%s' for write: %s", entry.ns, esp_err_to_name(ret));
        return ret;
    }

    switch (entry.type)
    {
        case NVS_TYPE_U8:   ret = nvs_set_u8(handle, entry.key, *reinterpret_cast<const uint8_t*>(entry.data)); break;
        case NVS_TYPE_I8:   ret = nvs_set_i8(handle, entry.key, *reinterpret_cast<const int8_t*>(entry.data)); break;
        case NVS_TYPE_U16:  ret = nvs_set_u16(handle, entry.key, *reinterpret_cast<const uint16_t*>(entry.data)); break;
        case NVS_TYPE_I16:  ret = nvs_set_i16(handle, entry.key, *reinterpret_cast<const int16_t*>(entry.data)); break;
        case NVS_TYPE_U32:  ret = nvs_set_u32(handle, entry.key, *reinterpret_cast<const uint32_t*>(entry.data)); break;
        case NVS_TYPE_I32:  ret = nvs_set_i32(handle, entry.key, *reinterpret_cast<const int32_t*>(entry.data)); break;
        case NVS_TYPE_U64:  ret = nvs_set_u64(handle, entry.key, *reinterpret_cast<const uint64_t*>(entry.data)); break;
        case NVS_TYPE_I64:  ret = nvs_set_i64(handle, entry.key, *reinterpret_cast<const int64_t*>(entry.data)); break;
        case NVS_TYPE_STR:  ret = nvs_set_str(handle, entry.key, reinterpret_cast<const char*>(entry.data)); break;
        case NVS_TYPE_BLOB: ret = nvs_set_blob(handle, entry.key, entry.data, entry.dataLen); break;
        default:            ret = ESP_ERR_INVALID_ARG; break;
    }

    if (ret == ESP_OK)
    {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);
    return ret;
}

bool isFactoryResetEnabled()
{
    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK)
    {
        return false;
    }

    uint8_t enabled = 0;
    nvs_get_u8(handle, kEnabledKey, &enabled);
    nvs_close(handle);
    return enabled == 1;
}

esp_err_t performFactoryReset()
{
    ESP_LOGI(kTag, "Starting factory reset...");

    // 1. Read the preserve list
    char preserveList[kMaxValueSize] = {};
    {
        nvs_handle_t handle;
        esp_err_t ret = nvs_open(kNamespace, NVS_READONLY, &handle);
        if (ret == ESP_OK)
        {
            size_t len = sizeof(preserveList);
            ret = nvs_get_str(handle, kPreserveKey, preserveList, &len);
            nvs_close(handle);
            if (ret != ESP_OK)
            {
                ESP_LOGI(kTag, "No preserve list found — wiping everything");
                preserveList[0] = '\0';
            }
        }
    }

    // 2. Parse and back up preserved entries
    std::vector<PreservedEntry> backed;

    if (preserveList[0] != '\0')
    {
        char* saveptr = nullptr;
        char* token = strtok_r(preserveList, ",", &saveptr);
        while (token != nullptr)
        {
            // Trim whitespace
            while (*token == ' ') token++;

            // Parse "namespace:key:type"
            char* colonA = strchr(token, ':');
            if (colonA == nullptr) { token = strtok_r(nullptr, ",", &saveptr); continue; }
            *colonA = '\0';
            char* keyStr = colonA + 1;

            char* colonB = strchr(keyStr, ':');
            if (colonB == nullptr) { token = strtok_r(nullptr, ",", &saveptr); continue; }
            *colonB = '\0';
            char* typeStr = colonB + 1;

            PreservedEntry entry = {};
            strncpy(entry.ns, token, sizeof(entry.ns) - 1);
            strncpy(entry.key, keyStr, sizeof(entry.key) - 1);
            entry.type = parseType(typeStr);

            esp_err_t ret = readEntry(entry);
            if (ret == ESP_OK)
            {
                ESP_LOGI(kTag, "  Backed up %s:%s (type=%d, %u bytes)",
                         entry.ns, entry.key, entry.type, (unsigned)entry.dataLen);
                backed.push_back(entry);
            }
            else
            {
                ESP_LOGW(kTag, "  Skip %s:%s — %s", entry.ns, entry.key, esp_err_to_name(ret));
            }

            token = strtok_r(nullptr, ",", &saveptr);
        }
    }

    // 3. Erase NVS
    ESP_LOGI(kTag, "Erasing NVS...");
    esp_err_t ret = nvs_flash_erase();
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "NVS erase failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 4. Reinitialize NVS
    ret = nvs_flash_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "NVS reinit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 5. Restore preserved entries
    for (const auto& entry : backed)
    {
        esp_err_t wret = writeEntry(entry);
        if (wret == ESP_OK)
        {
            ESP_LOGI(kTag, "  Restored %s:%s", entry.ns, entry.key);
        }
        else
        {
            ESP_LOGW(kTag, "  Failed to restore %s:%s — %s", entry.ns, entry.key, esp_err_to_name(wret));
        }
    }

    ESP_LOGI(kTag, "Factory reset complete — %d keys preserved", (int)backed.size());
    return ESP_OK;
}

}  // namespace safemode
```

- [ ] **Step 4: Build to verify**

Run: `python scripts/build.py --skip-frontend`
Expected: Build still fails on `setNvsAvailable` (Task 4 fixes this), but the factory_reset and ota components should compile clean. Check output for errors in those specific components.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/factory_reset/
git commit -m "feat: add factory reset component with NVS preserve/restore"
```

---

### Task 4: Update HTTP Server — Factory Reset Endpoint + Fixes

**Files:**
- Modify: `firmware/components/wifi/include/http_server.h`
- Modify: `firmware/components/wifi/src/http_server.cpp`
- Modify: `firmware/components/wifi/CMakeLists.txt`

- [ ] **Step 1: Update http_server.h**

Add `setNvsAvailable`, factory reset handler, and fix boot-into-app to not hardcode "app" label.

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
    void setNvsAvailable(bool available) { nvsAvailable_ = available; }

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
    bool nvsAvailable_ = false;

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
    static esp_err_t handleFactoryReset(httpd_req_t* req);

    // Helpers
    static void sendJsonOk(httpd_req_t* req);
    static void sendJsonError(httpd_req_t* req, int status);

    static void rebootTimerCallback(void* arg);
};

}  // namespace safemode
```

- [ ] **Step 2: Update http_server.cpp**

Three changes: (a) register `/api/factory-reset` route, (b) add `factoryResetEnabled` to `handleInfo`, (c) fix `handleBootApp` to find app partition dynamically instead of hardcoding "app" label, (d) implement `handleFactoryReset`.

In `registerRoutes()`, add before the SPA fallback:

```cpp
    httpd_uri_t factoryReset = {};
    factoryReset.uri = "/api/factory-reset";
    factoryReset.method = HTTP_POST;
    factoryReset.handler = handleFactoryReset;
    factoryReset.user_ctx = this;
    httpd_register_uri_handler(handle_, &factoryReset);
```

Add `#include "factory_reset.h"` to the top of http_server.cpp.

Replace `handleBootApp`:

```cpp
esp_err_t HttpServer::handleBootApp(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: boot into app");
    auto* server = static_cast<HttpServer*>(req->user_ctx);
    setCorsHeaders(req);

    // Find the first app partition not labeled "safemode"
    const esp_partition_t* appPartition = nullptr;
    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    while (it != nullptr)
    {
        const esp_partition_t* p = esp_partition_get(it);
        if (strcmp(p->label, "safemode") != 0)
        {
            appPartition = p;
            break;
        }
        it = esp_partition_next(it);
    }
    if (it != nullptr)
    {
        esp_partition_iterator_release(it);
    }

    if (appPartition == nullptr)
    {
        ESP_LOGE(kTag, "No app partition found");
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
```

Replace `handleInfo` — add `factoryResetEnabled` field and dynamic app partition lookup:

```cpp
esp_err_t HttpServer::handleInfo(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: info");
    auto* server = static_cast<HttpServer*>(req->user_ctx);
    setCorsHeaders(req);

    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);

    const esp_app_desc_t* appDesc = esp_app_get_description();
    const esp_partition_t* running = esp_ota_get_running_partition();

    // Find app partition dynamically
    const char* appLabel = "none";
    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    while (it != nullptr)
    {
        const esp_partition_t* p = esp_partition_get(it);
        if (strcmp(p->label, "safemode") != 0)
        {
            appLabel = p->label;
            break;
        }
        it = esp_partition_next(it);
    }
    if (it != nullptr)
    {
        esp_partition_iterator_release(it);
    }

    bool factoryResetEnabled = server->nvsAvailable_ && safemode::isFactoryResetEnabled();

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
        "\"firmwareVersion\":\"%s\","
        "\"factoryResetEnabled\":%s"
        "}",
        CONFIG_IDF_TARGET,
        chipInfo.revision,
        chipInfo.cores,
        esp_get_idf_version(),
        esp_get_free_heap_size(),
        running ? running->label : "unknown",
        appLabel,
        appDesc ? appDesc->version : "unknown",
        factoryResetEnabled ? "true" : "false"
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, len);
    return ESP_OK;
}
```

Add `handleFactoryReset`:

```cpp
esp_err_t HttpServer::handleFactoryReset(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: factory reset");
    auto* server = static_cast<HttpServer*>(req->user_ctx);
    setCorsHeaders(req);

    if (!server->nvsAvailable_ || !safemode::isFactoryResetEnabled())
    {
        ESP_LOGW(kTag, "Factory reset not available");
        sendJsonError(req, 400);
        return ESP_OK;
    }

    esp_err_t ret = safemode::performFactoryReset();
    if (ret != ESP_OK)
    {
        sendJsonError(req, 500);
        return ESP_OK;
    }

    sendJsonOk(req);
    return ESP_OK;
}
```

- [ ] **Step 3: Update wifi CMakeLists.txt**

Add `factory_reset` to PRIV_REQUIRES so the HTTP server can call `isFactoryResetEnabled()` and `performFactoryReset()`.

Find the `PRIV_REQUIRES` line and add `factory_reset`:

```cmake
    PRIV_REQUIRES
        ota
        esp_timer
        app_update
        factory_reset
```

- [ ] **Step 4: Build to verify**

Run: `python scripts/build.py --skip-frontend`
Expected: Full successful build. All components compile and link.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/wifi/
git commit -m "feat: add factory reset endpoint and dynamic partition lookup"
```

---

### Task 5: Update Frontend — Factory Reset UI

**Files:**
- Modify: `firmware/frontend/src/api.ts`
- Modify: `firmware/frontend/src/App.tsx`

- [ ] **Step 1: Update api.ts**

Add `factoryResetEnabled` to `DeviceInfo` and add `factoryReset()` function:

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
  factoryResetEnabled: boolean;
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

export async function factoryReset(): Promise<boolean> {
  const res = await fetch(apiUrl("/api/factory-reset"), { method: "POST" });
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

- [ ] **Step 2: Update App.tsx**

Add factory reset button, confirmation, and post-reset reboot options:

```tsx
import { useCallback, useEffect, useRef, useState } from "react";
import {
  ping,
  restart,
  bootIntoApp,
  uploadFirmware,
  factoryReset,
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
  const [showRebootOptions, setShowRebootOptions] = useState(false);
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

  const handleFactoryReset = useCallback(async () => {
    if (!confirm("This will erase all device settings. Continue?")) return;
    setMessage(null);
    try {
      const ok = await factoryReset();
      if (ok) {
        setMessage({ text: "Factory reset complete.", type: "success" });
        setShowRebootOptions(true);
      } else {
        setMessage({ text: "Factory reset failed.", type: "error" });
      }
    } catch {
      setMessage({ text: "Factory reset error. Check your connection.", type: "error" });
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

      {/* Post-reset reboot options */}
      {showRebootOptions && (
        <div className="mb-4 flex gap-3">
          <button
            onClick={async () => { setShowRebootOptions(false); await bootIntoApp(); setMessage({ text: "Rebooting into app...", type: "success" }); }}
            className="flex-1 rounded-lg bg-orange-500 px-4 py-2.5 text-sm font-semibold text-white transition-colors hover:bg-orange-400"
          >
            Reboot into App
          </button>
          <button
            onClick={async () => { setShowRebootOptions(false); await restart(); setMessage({ text: "Restarting in safemode...", type: "success" }); }}
            className="flex-1 rounded-lg bg-stone-700 px-4 py-2.5 text-sm font-medium text-stone-200 transition-colors hover:bg-stone-600"
          >
            Stay in Safemode
          </button>
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
        {deviceInfo?.factoryResetEnabled && (
          <button
            onClick={handleFactoryReset}
            disabled={uploading || !connected}
            className="mt-3 w-full rounded-lg bg-red-900/50 px-4 py-2.5 text-sm font-medium text-red-300 transition-colors hover:bg-red-900/70 disabled:cursor-not-allowed disabled:opacity-40"
          >
            Factory Reset
          </button>
        )}
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

- [ ] **Step 3: Build full project**

Run: `python scripts/build.py`
Expected: Firmware and frontend both build successfully.

- [ ] **Step 4: Commit**

```bash
git add firmware/frontend/src/api.ts firmware/frontend/src/App.tsx
git commit -m "feat: add factory reset UI with preserve-key support"
```

---

### Task 6: Final Build + Verify

- [ ] **Step 1: Clean build**

```bash
python scripts/clean.py --all
python scripts/build.py
```

Expected: Full clean build succeeds. Binary fits in safemode partition (< 896KB).

- [ ] **Step 2: Commit all remaining changes**

```bash
git add -A
git commit -m "feat: universal partition-agnostic safemode with factory reset"
```
