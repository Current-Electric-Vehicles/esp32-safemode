#include "ota_updater.h"

#include "esp_log.h"

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

    ESP_LOGI(kTag, "Starting OTA update to partition '%s' @ 0x%lx (%lu bytes)",
             partition_->label, partition_->address, partition_->size);

    esp_err_t ret = esp_ota_begin(partition_, OTA_SIZE_UNKNOWN, &handle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "esp_ota_begin failed: %s", esp_err_to_name(ret));
        partition_ = nullptr;
        return ret;
    }

    active_ = true;
    return ESP_OK;
}

esp_err_t OtaUpdater::write(const void* data, size_t len)
{
    if (!active_)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_ota_write(handle_, data, len);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "esp_ota_write failed: %s", esp_err_to_name(ret));
        abort();
        return ret;
    }

    return ESP_OK;
}

esp_err_t OtaUpdater::finish()
{
    if (!active_)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_ota_end(handle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "esp_ota_end failed: %s", esp_err_to_name(ret));
        abort();
        return ret;
    }

    ret = esp_ota_set_boot_partition(partition_);
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
        esp_ota_abort(handle_);
        ESP_LOGW(kTag, "OTA update aborted");
    }
    active_ = false;
    handle_ = 0;
    partition_ = nullptr;
}

}  // namespace safemode
