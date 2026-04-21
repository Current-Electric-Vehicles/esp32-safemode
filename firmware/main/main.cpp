#include "dns_server.h"
#include "esp_log.h"
#include "http_server.h"
#include "nvs_flash.h"
#include "ota_updater.h"
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

    // Start WiFi AP
    ret = safemode::WifiAp::start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to start WiFi AP: %s", esp_err_to_name(ret));
        return;
    }

    // Start DNS server (captive portal)
    ret = safemode::dnsServerStart();
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to start DNS server: %s", esp_err_to_name(ret));
    }

    // Create OTA updater
    safemode::OtaUpdater otaUpdater;

    // Start HTTP server
    safemode::HttpServer httpServer;
    httpServer.setOtaUpdater(&otaUpdater);

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
