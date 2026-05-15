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

    /// Current byte offset in the target partition. Equals total bytes written
    /// since begin(). Used by chunked upload to validate chunk ordering.
    size_t writeOffset() const { return writeOffset_; }

private:
    static constexpr const char* kTag = "ota";

    const esp_partition_t* targetPartition_ = nullptr;
    const esp_partition_t* partition_ = nullptr;
    bool active_ = false;
    bool encrypted_ = false;
    size_t writeOffset_ = 0;
};

}  // namespace safemode
