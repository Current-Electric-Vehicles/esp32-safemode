#include "factory_reset.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <cstring>
#include <utility>
#include <vector>

namespace safemode
{

static constexpr const char* kTag = "factory_reset";
static constexpr const char* kNamespace = "safemode";
// NVS key names are limited to NVS_KEY_NAME_MAX_SIZE-1 = 15 characters; longer
// names get rejected with ESP_ERR_NVS_KEY_TOO_LONG and silently never store.
// The earlier "factoryResetEnabled" / "factoryResetPreserve" names exceeded
// the limit, so nothing ever got written and the reset button never appeared.
static constexpr const char* kEnabledKey = "frEnabled";
static constexpr const char* kPreserveKey = "frPreserve";
static constexpr size_t kMaxValueSize = 4000;

// Heap-sized data buffer rather than a 4000-byte fixed array: a per-entry
// stack/vector footprint of ~4 KB exhausted both stack and heap on devices
// with several preserved keys (8+ entries × ~4 KB through vector reallocation
// = ~64 KB heap pressure plus a 4 KB stack frame per loop iteration).
struct PreservedEntry
{
    char ns[16];
    char key[16];
    nvs_type_t type;
    size_t dataLen;
    std::vector<uint8_t> data;
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

    switch (entry.type)
    {
        case NVS_TYPE_U8:   entry.data.resize(1); ret = nvs_get_u8 (handle, entry.key, reinterpret_cast<uint8_t* >(entry.data.data())); entry.dataLen = 1; break;
        case NVS_TYPE_I8:   entry.data.resize(1); ret = nvs_get_i8 (handle, entry.key, reinterpret_cast<int8_t*  >(entry.data.data())); entry.dataLen = 1; break;
        case NVS_TYPE_U16:  entry.data.resize(2); ret = nvs_get_u16(handle, entry.key, reinterpret_cast<uint16_t*>(entry.data.data())); entry.dataLen = 2; break;
        case NVS_TYPE_I16:  entry.data.resize(2); ret = nvs_get_i16(handle, entry.key, reinterpret_cast<int16_t* >(entry.data.data())); entry.dataLen = 2; break;
        case NVS_TYPE_U32:  entry.data.resize(4); ret = nvs_get_u32(handle, entry.key, reinterpret_cast<uint32_t*>(entry.data.data())); entry.dataLen = 4; break;
        case NVS_TYPE_I32:  entry.data.resize(4); ret = nvs_get_i32(handle, entry.key, reinterpret_cast<int32_t* >(entry.data.data())); entry.dataLen = 4; break;
        case NVS_TYPE_U64:  entry.data.resize(8); ret = nvs_get_u64(handle, entry.key, reinterpret_cast<uint64_t*>(entry.data.data())); entry.dataLen = 8; break;
        case NVS_TYPE_I64:  entry.data.resize(8); ret = nvs_get_i64(handle, entry.key, reinterpret_cast<int64_t* >(entry.data.data())); entry.dataLen = 8; break;
        case NVS_TYPE_STR:
        {
            // Two-step: query length, then read into a right-sized buffer.
            size_t len = 0;
            ret = nvs_get_str(handle, entry.key, nullptr, &len);
            if (ret == ESP_OK && len > 0 && len <= kMaxValueSize)
            {
                entry.data.resize(len);
                ret = nvs_get_str(handle, entry.key, reinterpret_cast<char*>(entry.data.data()), &len);
                entry.dataLen = len;
            }
            else if (ret == ESP_OK)
            {
                ret = ESP_ERR_NVS_INVALID_LENGTH;
            }
            break;
        }
        case NVS_TYPE_BLOB:
        {
            size_t len = 0;
            ret = nvs_get_blob(handle, entry.key, nullptr, &len);
            if (ret == ESP_OK && len > 0 && len <= kMaxValueSize)
            {
                entry.data.resize(len);
                ret = nvs_get_blob(handle, entry.key, entry.data.data(), &len);
                entry.dataLen = len;
            }
            else if (ret == ESP_OK)
            {
                ret = ESP_ERR_NVS_INVALID_LENGTH;
            }
            break;
        }
        default: ret = ESP_ERR_INVALID_ARG; break;
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
        case NVS_TYPE_U8:   ret = nvs_set_u8 (handle, entry.key, *reinterpret_cast<const uint8_t* >(entry.data.data())); break;
        case NVS_TYPE_I8:   ret = nvs_set_i8 (handle, entry.key, *reinterpret_cast<const int8_t*  >(entry.data.data())); break;
        case NVS_TYPE_U16:  ret = nvs_set_u16(handle, entry.key, *reinterpret_cast<const uint16_t*>(entry.data.data())); break;
        case NVS_TYPE_I16:  ret = nvs_set_i16(handle, entry.key, *reinterpret_cast<const int16_t* >(entry.data.data())); break;
        case NVS_TYPE_U32:  ret = nvs_set_u32(handle, entry.key, *reinterpret_cast<const uint32_t*>(entry.data.data())); break;
        case NVS_TYPE_I32:  ret = nvs_set_i32(handle, entry.key, *reinterpret_cast<const int32_t* >(entry.data.data())); break;
        case NVS_TYPE_U64:  ret = nvs_set_u64(handle, entry.key, *reinterpret_cast<const uint64_t*>(entry.data.data())); break;
        case NVS_TYPE_I64:  ret = nvs_set_i64(handle, entry.key, *reinterpret_cast<const int64_t* >(entry.data.data())); break;
        case NVS_TYPE_STR:  ret = nvs_set_str(handle, entry.key, reinterpret_cast<const char*>(entry.data.data())); break;
        case NVS_TYPE_BLOB: ret = nvs_set_blob(handle, entry.key, entry.data.data(), entry.dataLen); break;
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
    // Pre-reserve roughly the comma count + 1 so we don't pay for the
    // doubling-and-move-construct on every push_back.
    {
        size_t commas = 0;
        for (const char* p = preserveList; *p; ++p) if (*p == ',') commas++;
        backed.reserve(commas + 1);
    }

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

            PreservedEntry entry;
            strncpy(entry.ns, token, sizeof(entry.ns) - 1);
            entry.ns[sizeof(entry.ns) - 1] = '\0';
            strncpy(entry.key, keyStr, sizeof(entry.key) - 1);
            entry.key[sizeof(entry.key) - 1] = '\0';
            entry.type = parseType(typeStr);
            entry.dataLen = 0;

            esp_err_t ret = readEntry(entry);
            if (ret == ESP_OK)
            {
                ESP_LOGI(kTag, "  Backed up %s:%s (type=%d, %u bytes)",
                         entry.ns, entry.key, entry.type, (unsigned)entry.dataLen);
                backed.push_back(std::move(entry));
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
