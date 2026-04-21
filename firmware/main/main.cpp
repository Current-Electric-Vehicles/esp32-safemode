#include "esp_log.h"
#include "nvs_flash.h"

static constexpr const char* kTag = "safemode";

extern "C" void app_main()
{
    ESP_LOGI(kTag, "Safemode starting...");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(kTag, "NVS partition truncated, erasing...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(kTag, "Safemode initialized (stub)");

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
