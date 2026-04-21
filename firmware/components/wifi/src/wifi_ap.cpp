#include "wifi_ap.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"

#include <cstring>

namespace safemode
{

esp_err_t WifiAp::start()
{
    ESP_LOGI(kTag, "Starting WiFi AP: SSID=%s", kSsid);

    // Initialize network interface and event loop
    ESP_RETURN_ON_ERROR(esp_netif_init(), kTag, "netif init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), kTag, "event loop");

    // Create AP netif and configure static IP
    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();

    // Stop DHCP server to change IP
    esp_netif_dhcps_stop(ap_netif);

    esp_netif_ip_info_t ip_info = {};
    IP4_ADDR(&ip_info.ip, 4, 3, 2, 1);
    IP4_ADDR(&ip_info.gw, 4, 3, 2, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    ESP_RETURN_ON_ERROR(esp_netif_set_ip_info(ap_netif, &ip_info), kTag, "set ip");

    // Restart DHCP server with new IP range
    ESP_RETURN_ON_ERROR(esp_netif_dhcps_start(ap_netif), kTag, "dhcps start");

    // Initialize WiFi driver
    wifi_init_config_t wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifiInitConfig), kTag, "wifi init");

    // Configure AP
    wifi_config_t wifiConfig = {};
    std::strncpy(reinterpret_cast<char*>(wifiConfig.ap.ssid), kSsid, sizeof(wifiConfig.ap.ssid));
    wifiConfig.ap.ssid_len = std::strlen(kSsid);
    std::strncpy(reinterpret_cast<char*>(wifiConfig.ap.password), kPassword, sizeof(wifiConfig.ap.password));
    wifiConfig.ap.channel = kChannel;
    wifiConfig.ap.max_connection = kMaxConnections;
    wifiConfig.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), kTag, "set mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifiConfig), kTag, "set config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), kTag, "wifi start");

    ESP_LOGI(kTag, "WiFi AP started: SSID=%s, IP=4.3.2.1, channel=%d", kSsid, kChannel);
    return ESP_OK;
}

esp_err_t WifiAp::stop()
{
    ESP_LOGI(kTag, "Stopping WiFi AP");
    ESP_RETURN_ON_ERROR(esp_wifi_stop(), kTag, "wifi stop");
    ESP_RETURN_ON_ERROR(esp_wifi_deinit(), kTag, "wifi deinit");
    return ESP_OK;
}

}  // namespace safemode
