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
