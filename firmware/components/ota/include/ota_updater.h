#pragma once

#include "esp_err.h"
#include "esp_ota_ops.h"
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

    /// Begin an OTA update. Finds the "app" partition and calls esp_ota_begin().
    esp_err_t begin();

    /// Write a chunk of firmware data.
    esp_err_t write(const void* data, size_t len);

    /// Finish the update. Calls esp_ota_end() and sets the boot partition.
    esp_err_t finish();

    /// Abort an in-progress update and clean up.
    void abort();

    /// Returns true if an update is in progress.
    bool isActive() const { return active_; }

private:
    static constexpr const char* kTag = "ota";
    static constexpr const char* kAppPartitionLabel = "app";

    const esp_partition_t* partition_ = nullptr;
    esp_ota_handle_t handle_ = 0;
    bool active_ = false;
};

}  // namespace safemode
