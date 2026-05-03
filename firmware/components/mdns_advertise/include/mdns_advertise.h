#pragma once

#include "esp_err.h"

namespace safemode
{

class MdnsAdvertise
{
public:
    /// Initialize mDNS and advertise safemode services on the AP netif.
    /// Hostname is derived from the device MAC ("safemode-xxxxxx"), so
    /// each device exposes a unique `.local` name. Advertises:
    ///   - _http._tcp:80     (browser/curl users)
    ///   - _safemode._tcp:80 with TXT records devid, fwver, name — companion
    ///     apps browse this service type to discover devices in recovery.
    static esp_err_t start();

    /// Tear down mDNS.
    static esp_err_t stop();

private:
    static constexpr const char* kTag = "mdns_advertise";
    static constexpr const char* kInstanceName = "SAFEMODE Recovery";
    static constexpr int kHttpPort = 80;
};

}  // namespace safemode
