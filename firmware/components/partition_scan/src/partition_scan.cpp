#include "partition_scan.h"

#include "esp_flash.h"
#include "esp_flash_encrypt.h"
#include "esp_log.h"
#include "esp_partition.h"

#include <cstring>

namespace safemode
{

static constexpr const char* kTag = "partition_scan";
static constexpr uint16_t kPartitionMagic = 0x50AA;
static constexpr uint16_t kMd5Magic = 0xEBEB;
static constexpr size_t kEntrySize = 32;
static constexpr size_t kMaxEntries = 20;

struct __attribute__((packed)) RawPartitionEntry
{
    uint16_t magic;
    uint8_t type;
    uint8_t subtype;
    uint32_t offset;
    uint32_t size;
    char label[16];
    uint32_t flags;
};

esp_err_t scanAndRegisterPartitions()
{
    bool encrypted = esp_flash_encryption_enabled();
    ESP_LOGI(kTag, "Flash encryption: %s", encrypted ? "ENABLED" : "disabled");

    // Scan 4KB-aligned offsets for partition table magic byte.
    // Use encrypted read if flash encryption is active.
    uint32_t tableOffset = 0;
    for (uint32_t addr = 0x4000; addr <= 0x20000; addr += 0x1000)
    {
        uint16_t magic = 0;
        esp_err_t ret;
        if (encrypted)
        {
            ret = esp_flash_read_encrypted(nullptr, addr, &magic, sizeof(magic));
        }
        else
        {
            ret = esp_flash_read(nullptr, &magic, addr, sizeof(magic));
        }
        if (ret != ESP_OK)
        {
            continue;
        }

        // Log what we find at key offsets for debugging
        if (addr == 0x8000 || addr == 0x9000 || addr == 0x10000)
        {
            ESP_LOGI(kTag, "  Probe 0x%lx: magic=0x%04x %s",
                     (unsigned long)addr, magic,
                     (magic == kPartitionMagic) ? "<-- MATCH" : "");
        }

        if (magic == kPartitionMagic)
        {
            tableOffset = addr;
            break;
        }
    }

    if (tableOffset == 0)
    {
        ESP_LOGE(kTag, "No partition table found in flash (scanned 0x4000–0x20000)");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(kTag, "Found partition table at 0x%lx", (unsigned long)tableOffset);

    // Parse and register each entry
    size_t registered = 0;
    for (size_t i = 0; i < kMaxEntries; i++)
    {
        RawPartitionEntry entry;
        uint32_t addr = tableOffset + i * kEntrySize;
        esp_err_t readRet;
        if (encrypted)
        {
            readRet = esp_flash_read_encrypted(nullptr, addr, &entry, sizeof(entry));
        }
        else
        {
            readRet = esp_flash_read(nullptr, &entry, addr, sizeof(entry));
        }
        if (readRet != ESP_OK)
        {
            break;
        }

        // End-of-table markers
        if (entry.magic == 0xFFFF || entry.magic == kMd5Magic)
        {
            break;
        }
        if (entry.magic != kPartitionMagic)
        {
            continue;
        }

        // Null-terminate label
        char label[17] = {};
        std::memcpy(label, entry.label, 16);

        ESP_LOGI(kTag, "  %-12s type=%d sub=0x%02x off=0x%lx size=0x%lx",
                 label, entry.type, entry.subtype,
                 (unsigned long)entry.offset, (unsigned long)entry.size);

        // Check if this partition was already loaded natively by ESP-IDF
        // (happens when CONFIG_PARTITION_TABLE_OFFSET matches the device)
        const esp_partition_t* existing = esp_partition_find_first(
            static_cast<esp_partition_type_t>(entry.type),
            static_cast<esp_partition_subtype_t>(entry.subtype),
            label);

        if (existing != nullptr)
        {
            registered++;
            continue;
        }

        const esp_partition_t* out = nullptr;
        esp_err_t ret = esp_partition_register_external(
            nullptr,  // main internal flash
            entry.offset,
            entry.size,
            label,
            static_cast<esp_partition_type_t>(entry.type),
            static_cast<esp_partition_subtype_t>(entry.subtype),
            &out);

        if (ret == ESP_OK)
        {
            registered++;
        }
        else
        {
            ESP_LOGW(kTag, "  Failed to register '%s': %s", label, esp_err_to_name(ret));
        }
    }

    ESP_LOGI(kTag, "Registered %d partitions", (int)registered);
    return registered > 0 ? ESP_OK : ESP_ERR_NOT_FOUND;
}

}  // namespace safemode
