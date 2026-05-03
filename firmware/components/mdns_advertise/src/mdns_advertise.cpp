#include "mdns_advertise.h"

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mdns.h"

#include <cstdio>

namespace safemode
{

namespace
{

void buildHostname(char* out, size_t outLen)
{
    uint8_t mac[6] = {};
    esp_efuse_mac_get_default(mac);
    std::snprintf(out, outLen, "safemode-%02x%02x%02x", mac[3], mac[4], mac[5]);
}

void buildDeviceId(char* out, size_t outLen)
{
    uint8_t mac[6] = {};
    esp_efuse_mac_get_default(mac);
    std::snprintf(out, outLen, "%02x%02x%02x%02x%02x%02x",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

}  // namespace

esp_err_t MdnsAdvertise::start()
{
    char hostname[32] = {};
    char devid[16] = {};
    buildHostname(hostname, sizeof(hostname));
    buildDeviceId(devid, sizeof(devid));

    const esp_app_desc_t* appDesc = esp_app_get_description();
    const char* fwver = (appDesc && appDesc->version[0]) ? appDesc->version : "unknown";

    ESP_LOGI(kTag, "Starting mDNS: %s.local (%s)", hostname, kInstanceName);

    esp_err_t err = mdns_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "mdns_init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = mdns_hostname_set(hostname);
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "mdns_hostname_set failed: %s", esp_err_to_name(err));
        return err;
    }

    err = mdns_instance_name_set(kInstanceName);
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "mdns_instance_name_set failed: %s", esp_err_to_name(err));
        return err;
    }

    err = mdns_service_add(kInstanceName, "_http", "_tcp", kHttpPort, nullptr, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "mdns_service_add(_http) failed: %s", esp_err_to_name(err));
        return err;
    }

    // _safemode._tcp is the discriminator: companion apps browse this
    // service type to find devices in recovery, with no HTTP probe needed.
    mdns_txt_item_t safemodeTxt[] = {
        { "devid", devid      },
        { "fwver", fwver      },
        { "name",  kInstanceName },
    };
    err = mdns_service_add(kInstanceName, "_safemode", "_tcp", kHttpPort,
                           safemodeTxt, sizeof(safemodeTxt) / sizeof(safemodeTxt[0]));
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "mdns_service_add(_safemode) failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "mDNS started: %s.local (services: _http._tcp, _safemode._tcp)", hostname);
    return ESP_OK;
}

esp_err_t MdnsAdvertise::stop()
{
    mdns_free();
    return ESP_OK;
}

}  // namespace safemode
