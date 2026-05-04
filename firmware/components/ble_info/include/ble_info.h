#pragma once

#include "esp_err.h"

#include <string>

namespace safemode
{
namespace ble
{

/// Information broadcast over BLE so that apps can discover safemode devices
/// without joining the WiFi AP. Read-only, no pairing required.
struct Info
{
    std::string deviceName;       // BLE advertised name (e.g. "SAFEMODE")
    std::string ssid;             // WiFi AP SSID
    std::string password;         // WiFi AP password (info only — anyone in range can read)
    std::string ipAddress;        // AP gateway IP
    std::string firmwareVersion;  // safemode firmware version
};

/// Start advertising the safemode info service.
/// Service UUID: 5afe0000-2026-4d3e-b9c1-7fa8c4d6e8a1
/// Read-only characteristics (each returns a plain UTF-8 string):
///   SSID:     5afe0001-2026-4d3e-b9c1-7fa8c4d6e8a1
///   Password: 5afe0002-2026-4d3e-b9c1-7fa8c4d6e8a1
///   IP:       5afe0003-2026-4d3e-b9c1-7fa8c4d6e8a1
///   Version:  5afe0004-2026-4d3e-b9c1-7fa8c4d6e8a1
esp_err_t startInfoBroadcast(const Info& info);

/// Stop advertising and shut down the BLE stack.
esp_err_t stopInfoBroadcast();

}  // namespace ble
}  // namespace safemode
