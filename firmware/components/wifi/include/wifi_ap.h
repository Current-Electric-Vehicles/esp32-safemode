#pragma once

#include "esp_err.h"

namespace safemode
{

class WifiAp
{
public:
    /// Start WiFi AP with hardcoded SSID "SAFEMODE", password "safemode", IP 4.3.2.1
    static esp_err_t start();

    /// Stop WiFi AP
    static esp_err_t stop();

private:
    static constexpr const char* kTag = "wifi_ap";
    static constexpr const char* kSsid = "SAFEMODE";
    static constexpr const char* kPassword = "safemode";
    static constexpr int kMaxConnections = 4;
    static constexpr int kChannel = 1;
};

}  // namespace safemode
