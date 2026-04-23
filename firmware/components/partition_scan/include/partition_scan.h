#pragma once

#include "esp_err.h"

namespace safemode
{

/// Scan flash for the partition table and register all found partitions
/// with ESP-IDF so that standard APIs (NVS, OTA, etc.) work normally.
/// Probes 4KB-aligned offsets from 0x4000 to 0x20000.
/// Returns ESP_OK if a partition table was found and partitions registered.
esp_err_t scanAndRegisterPartitions();

}  // namespace safemode
