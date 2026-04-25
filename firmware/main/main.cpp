#include "esp_log.h"
#include "http_server.h"
#include "nvs_flash.h"
#include "ota_updater.h"
#include "partition_scan.h"
#include "wifi_ap.h"

static constexpr const char* kTag = "safemode";

extern "C" void app_main()
{
    ESP_LOGI(kTag, "Safemode starting...");

    // Quiet noisy ESP-IDF components
    esp_log_level_set("httpd", ESP_LOG_WARN);
    esp_log_level_set("httpd_uri", ESP_LOG_WARN);
    esp_log_level_set("httpd_txrx", ESP_LOG_WARN);
    esp_log_level_set("httpd_parse", ESP_LOG_WARN);
    esp_log_level_set("httpd_sess", ESP_LOG_WARN);
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("wifi_init", ESP_LOG_WARN);
    esp_log_level_set("esp_netif_lwip", ESP_LOG_WARN);

    // Always scan flash for partition table — safemode is partition-agnostic.
    esp_err_t ret = safemode::scanAndRegisterPartitions();
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to discover partitions — cannot continue");
        return;
    }

    // Initialize NVS (best-effort — WiFi uses RAM-only storage as fallback)
    bool nvsAvailable = false;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(kTag, "NVS partition needs erase...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret == ESP_OK)
    {
        nvsAvailable = true;
    }
    else
    {
        ESP_LOGW(kTag, "NVS unavailable (%s), continuing without it", esp_err_to_name(ret));
    }

    // Start WiFi AP
    ret = safemode::WifiAp::start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to start WiFi AP: %s", esp_err_to_name(ret));
        return;
    }

    // Create OTA updater
    safemode::OtaUpdater otaUpdater;

    // Start HTTP server
    safemode::HttpServer httpServer;
    httpServer.setOtaUpdater(&otaUpdater);
    httpServer.setNvsAvailable(nvsAvailable);

    ret = httpServer.start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(kTag, "Safemode ready — connect to WiFi 'SAFEMODE' (password: safemode)");
    ESP_LOGI(kTag, "Open http://4.3.2.1 in your browser");

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
