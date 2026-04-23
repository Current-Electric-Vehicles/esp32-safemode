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

    /// Set the target partition for OTA updates. If not set, begin() will
    /// search for the first app partition not labeled "safemode".
    void setTargetPartition(const esp_partition_t* partition) { targetPartition_ = partition; }

    /// Begin an OTA update. Uses the target partition (if set) or finds one.
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

    const esp_partition_t* targetPartition_ = nullptr;
    const esp_partition_t* partition_ = nullptr;
    esp_ota_handle_t handle_ = 0;
    bool active_ = false;
};

}  // namespace safemode
