
#include "SafemodeWebServer.h"

#include <ESP.h>
#include <nvs_flash.h>
#include <esp_ota_ops.h>
#include <esp_log.h>
#include <esp_flash_encrypt.h>
#include <esp_partition.h>
#include <hal/spi_flash_types.h>
#include <spi_flash_chip_driver.h>

#include "StaticFiles.h"

static const char *TAG = "SafemodeWebServer";

#define _JSON_RESPONSE(_status, _result) \
    res.status(_status); \
    res.set("Content-Type", "application/json"); \
    res.print("{\"result\":" #_result "}"); \
    res.end();

#define JSON_RESPONSE_BODY(_status, _jsonObject) \
    res.status(_status); \
    res.set("Content-Type", "application/json"); \
    serializeJson(_jsonObject, res); \
    res.end();

#define JSON_RESPONSE_OK() _JSON_RESPONSE(200, true)
#define JSON_RESPONSE_404() _JSON_RESPONSE(404, false)
#define JSON_RESPONSE_400() _JSON_RESPONSE(400, false)
#define JSON_RESPONSE_500() _JSON_RESPONSE(500, false)

SafemodeWebServer SAFEMODE_WEBSERVER;

SafemodeWebServer::SafemodeWebServer(int port):
    wifiServer(80),
    webServer(),
    clients(),
    timeToLive() {
    
}

SafemodeWebServer::~SafemodeWebServer() {
    
}

void SafemodeWebServer::setup() {

    String ssid = "SAFEMODE";
    String pass = "safemode";
    ESP_LOGI(TAG, "Bringing up AP: %s, with pasdsword: %s, on http://4.3.2.1", ssid.c_str(), pass.c_str());

    WiFi.persistent(false);
    WiFi.enableAP(true);
    WiFi.softAPConfig(IPAddress(4, 3, 2, 1), IPAddress(4, 3, 2, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP(ssid, pass);

   webServer.use(
        [](Request &req, Response &res) {
            res.set("Connection", "keep-alive");
            res.set("Access-Control-Allow-Origin", "*");
            res.set("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, PATCH");
            res.set("Access-Control-Allow-Headers", "Content-Type, Content-Length, X-File-Size");
        });

    /**
     * Ping
     */
    webServer.post("/api/ping",
        [](Request &req, Response &res) {
            ESP_LOGI(TAG, "API Ping");
            JSON_RESPONSE_OK();
        });

    /**
     * Restart
     */
    webServer.post("/api/restart",
        [](Request &req, Response &res) {
            ESP_LOGI(TAG, "API Restart");
            JSON_RESPONSE_OK();
            ESP.restart();
        });

    /**
     * OTA Updates
     */
    webServer.post("/api/update",
        [](Request &req, Response &res) {
            ESP_LOGI(TAG, "API OTA Update");

            esp_log_level_set(TAG, ESP_LOG_DEBUG);

            req.setTimeout(10000);
\
            const esp_partition_t* update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, "app");
            if (update_partition == nullptr) {
                ESP_LOGE(TAG, "Unable to find partition with label 'app'");
                JSON_RESPONSE_500();
                return;
            }

            esp_flash_t* flash = update_partition->flash_chip;
            const uint32_t partAddr = update_partition->address;
            const uint32_t partSize = update_partition->size;
            const size_t sectorSize = flash->chip_drv->sector_size;
            uint8_t readBuffer[sectorSize];

            ESP_LOGD(TAG, "Found app partition with offset: 0x%x and size: %d", partAddr, partSize);

            esp_ota_handle_t update_handle;
            if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle) != ESP_OK) {
                ESP_LOGE(TAG, "Unable to esp_ota_begin");
                JSON_RESPONSE_500();
                return;
            }

            ESP_LOGI(TAG, "Beginning writes at: 0x%x for OTA update", partAddr);

            uint32_t totalBytesRead = 0;
            for (uint32_t addr = partAddr; addr < (partAddr + partSize); addr += sectorSize) {

                if (esp_flash_read(flash, &readBuffer[0], addr, sectorSize) != ESP_OK) {
                    ESP_LOGE(TAG, "Unable to esp_flash_read");
                    JSON_RESPONSE_500();
                    return;
                }

                uint32_t readBufferLen = min(sectorSize, partSize - (addr - partAddr));

                int chunkBytesRead = 0;
                while (chunkBytesRead < readBufferLen && req.left()) {
                    int read = req.read(&readBuffer[0] + chunkBytesRead, readBufferLen - chunkBytesRead);
                    if (read == -1) {
                        continue;
                    }
                    chunkBytesRead += read;
                }

                if (esp_flash_erase_region(flash, addr, sectorSize) != ESP_OK) {
                    ESP_LOGE(TAG, "Unable to esp_flash_erase_region");
                    JSON_RESPONSE_500();
                    return;
                }

                if (esp_flash_write(flash, &readBuffer[0], addr, sectorSize) != ESP_OK) {
                    ESP_LOGE(TAG, "Unable to esp_flash_write");
                    JSON_RESPONSE_500();
                    return;
                }
            }

            if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
                ESP_LOGE(TAG, "Unable to esp_ota_set_boot_partition");
                JSON_RESPONSE_500();
                return;
            }
            
            JSON_RESPONSE_OK();

            ESP_LOGI(TAG, "Update successful, restarting");
            delay(2000);
            ESP.restart();
        });

    webServer.options(
        [](Request &req, Response &res) {
            JSON_RESPONSE_OK();
        });

    webServer.use(staticFiles());

    /**
     * Start the server
     */
    wifiServer.begin();
}

void SafemodeWebServer::loop() {

  if (this->wifiServer.hasClient()) {
    for (int i = 0; i < this->clients.size(); i++) {
      if (!this->clients[i].connected()) {
        this->clients[i] = this->wifiServer.available();
        this->timeToLive[i] = millis() +  SAFEMODE_WEBSERVER_MAX_IDLE;
        break;
      }
    }
  }

  for (int i = 0; i < this->clients.size(); i++) {
    if (this->clients[i].available()) {
      this->webServer.process(&this->clients[i]);
      this->timeToLive[i] = millis() +  SAFEMODE_WEBSERVER_MAX_IDLE;
    } else if (this->timeToLive[i] && this->timeToLive[i] < millis()) {
      this->clients[i].stop();
      this->timeToLive[i] = 0;
    }
  }
}
