#include "dns_server.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#include <cstring>

namespace safemode
{

static constexpr const char* kTag = "dns";
static constexpr uint16_t kDnsPort = 53;
static constexpr size_t kDnsMaxPacket = 512;
static constexpr size_t kTaskStackSize = 4096;
static constexpr UBaseType_t kTaskPriority = 2;

// Captive portal IP: 4.3.2.1
static constexpr uint8_t kApIp[4] = {4, 3, 2, 1};

static TaskHandle_t sTask = nullptr;
static volatile bool sShouldRun = false;
static int sSock = -1;

static int buildResponse(const uint8_t* query, int queryLen, uint8_t* resp, int respMax)
{
    if (queryLen < 12 || respMax < queryLen + 16)
    {
        return -1;
    }

    std::memcpy(resp, query, queryLen);

    // Set response flags: QR=1, AA=1, RCODE=0
    resp[2] = 0x81;
    resp[3] = 0x80;

    // Set answer count to 1
    resp[6] = 0x00;
    resp[7] = 0x01;

    int pos = queryLen;

    // Name: pointer to the name in the question (offset 12)
    resp[pos++] = 0xC0;
    resp[pos++] = 0x0C;

    // Type: A (1)
    resp[pos++] = 0x00;
    resp[pos++] = 0x01;

    // Class: IN (1)
    resp[pos++] = 0x00;
    resp[pos++] = 0x01;

    // TTL: 60 seconds
    resp[pos++] = 0x00;
    resp[pos++] = 0x00;
    resp[pos++] = 0x00;
    resp[pos++] = 60;

    // RDLENGTH: 4 (IPv4)
    resp[pos++] = 0x00;
    resp[pos++] = 0x04;

    // RDATA: captive portal IP
    resp[pos++] = kApIp[0];
    resp[pos++] = kApIp[1];
    resp[pos++] = kApIp[2];
    resp[pos++] = kApIp[3];

    return pos;
}

static void dnsTask(void* arg)
{
    ESP_LOGI(kTag, "DNS server task started on port %d", kDnsPort);

    uint8_t queryBuf[kDnsMaxPacket];
    uint8_t respBuf[kDnsMaxPacket];

    while (sShouldRun)
    {
        struct sockaddr_in clientAddr = {};
        socklen_t addrLen = sizeof(clientAddr);

        int len = recvfrom(sSock, queryBuf, sizeof(queryBuf), 0,
                           reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);

        if (len < 0)
        {
            if (sShouldRun)
            {
                ESP_LOGW(kTag, "recvfrom error: %d", errno);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            continue;
        }

        int respLen = buildResponse(queryBuf, len, respBuf, sizeof(respBuf));
        if (respLen > 0)
        {
            sendto(sSock, respBuf, respLen, 0,
                   reinterpret_cast<struct sockaddr*>(&clientAddr), addrLen);
        }
    }

    ESP_LOGI(kTag, "DNS server task exiting");
    vTaskDelete(nullptr);
}

esp_err_t dnsServerStart()
{
    if (sTask)
    {
        return ESP_ERR_INVALID_STATE;
    }

    sSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sSock < 0)
    {
        ESP_LOGE(kTag, "Failed to create socket: %d", errno);
        return ESP_FAIL;
    }

    struct timeval tv = {};
    tv.tv_sec = 1;
    setsockopt(sSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kDnsPort);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sSock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        ESP_LOGE(kTag, "Failed to bind DNS socket: %d", errno);
        close(sSock);
        sSock = -1;
        return ESP_FAIL;
    }

    sShouldRun = true;
    BaseType_t created = xTaskCreate(dnsTask, "dns_srv", kTaskStackSize, nullptr, kTaskPriority, &sTask);
    if (created != pdPASS)
    {
        ESP_LOGE(kTag, "Failed to create DNS task");
        close(sSock);
        sSock = -1;
        sShouldRun = false;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(kTag, "DNS server started — all queries resolve to %d.%d.%d.%d",
             kApIp[0], kApIp[1], kApIp[2], kApIp[3]);
    return ESP_OK;
}

void dnsServerStop()
{
    if (!sTask)
        return;

    sShouldRun = false;

    if (sSock >= 0)
    {
        close(sSock);
        sSock = -1;
    }

    for (int i = 0; i < 20 && eTaskGetState(sTask) != eDeleted; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    sTask = nullptr;

    ESP_LOGI(kTag, "DNS server stopped");
}

}  // namespace safemode
