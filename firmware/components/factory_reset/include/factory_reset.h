#pragma once

#include "esp_err.h"

namespace safemode
{

/// Check if factory reset is enabled in the "safemode" NVS namespace.
/// Returns true if the "frEnabled" key is set to 1.
/// Returns false if NVS is unavailable or the key is missing/zero.
bool isFactoryResetEnabled();

/// Perform factory reset: read the preserve list from NVS, back up
/// listed keys, erase NVS, restore preserved keys.
/// Returns ESP_OK on success, error code on failure.
esp_err_t performFactoryReset();

}  // namespace safemode
