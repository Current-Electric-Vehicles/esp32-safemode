#include "http_server.h"

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "factory_reset.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota_updater.h"
#include "web_assets.h"

#include <cstdio>
#include <cstring>

namespace safemode
{

HttpServer::~HttpServer()
{
    stop();
}

esp_err_t HttpServer::start(uint16_t port)
{
    if (handle_)
    {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 16;
    config.max_open_sockets = 4;
    config.stack_size = 8192;
    config.recv_wait_timeout = 30;

    ESP_LOGI(kTag, "Starting HTTP server on port %d", port);
    esp_err_t ret = httpd_start(&handle_, &config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    registerRoutes();
    ESP_LOGI(kTag, "HTTP server started");
    return ESP_OK;
}

esp_err_t HttpServer::stop()
{
    if (!handle_)
        return ESP_OK;
    esp_err_t ret = httpd_stop(handle_);
    handle_ = nullptr;
    return ret;
}

// ---- JSON helpers ----

void HttpServer::sendJsonOk(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"result\":true}");
}

void HttpServer::sendJsonError(httpd_req_t* req, int status)
{
    httpd_resp_set_status(req, status == 404 ? "404 Not Found"
                             : status == 400 ? "400 Bad Request"
                                             : "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"result\":false}");
}

// ---- CORS ----

void HttpServer::setCorsHeaders(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, X-File-Size");
}

esp_err_t HttpServer::handleOptions(httpd_req_t* req)
{
    setCorsHeaders(req);
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

// ---- Reboot ----

void HttpServer::rebootTimerCallback(void* arg)
{
    ESP_LOGI("http_server", "Rebooting...");
    esp_restart();
}

void HttpServer::factoryResetTask(void* arg)
{
    // Delay to let the HTTP response flush
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI("http_server", "Performing factory reset...");
    // Stop WiFi first — the PHY background task accesses NVS calibration
    // data and will crash if we erase NVS while it's running.
    esp_wifi_stop();
    safemode::performFactoryReset();
    ESP_LOGI("http_server", "Factory reset done, rebooting...");
    esp_restart();
}

void HttpServer::scheduleReboot(uint32_t delayMs)
{
    ESP_LOGI(kTag, "Scheduling reboot in %lu ms", delayMs);
    const esp_timer_create_args_t timerArgs = {
        .callback = rebootTimerCallback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "reboot",
        .skip_unhandled_events = false,
    };
    esp_timer_handle_t timer = nullptr;
    if (esp_timer_create(&timerArgs, &timer) == ESP_OK)
    {
        esp_timer_start_once(timer, static_cast<uint64_t>(delayMs) * 1000);
    }
}

// ---- Routes ----

void HttpServer::registerRoutes()
{
    // Static files from embedded web assets
    for (int i = 0; i < kWebAssetCount; i++)
    {
        httpd_uri_t uri = {};
        uri.uri = kWebAssets[i].urlPath;
        uri.method = HTTP_GET;
        uri.handler = handleStaticFile;
        uri.user_ctx = this;
        httpd_register_uri_handler(handle_, &uri);
    }

    // CORS preflight
    httpd_uri_t opts = {};
    opts.uri = "/api/*";
    opts.method = HTTP_OPTIONS;
    opts.handler = handleOptions;
    opts.user_ctx = this;
    httpd_register_uri_handler(handle_, &opts);

    // API routes
    httpd_uri_t ping = {};
    ping.uri = "/api/ping";
    ping.method = HTTP_POST;
    ping.handler = handlePing;
    ping.user_ctx = this;
    httpd_register_uri_handler(handle_, &ping);

    httpd_uri_t restart = {};
    restart.uri = "/api/restart";
    restart.method = HTTP_POST;
    restart.handler = handleRestart;
    restart.user_ctx = this;
    httpd_register_uri_handler(handle_, &restart);

    httpd_uri_t bootApp = {};
    bootApp.uri = "/api/app";
    bootApp.method = HTTP_POST;
    bootApp.handler = handleBootApp;
    bootApp.user_ctx = this;
    httpd_register_uri_handler(handle_, &bootApp);

    httpd_uri_t update = {};
    update.uri = "/api/update";
    update.method = HTTP_POST;
    update.handler = handleUpdate;
    update.user_ctx = this;
    httpd_register_uri_handler(handle_, &update);

    httpd_uri_t info = {};
    info.uri = "/api/info";
    info.method = HTTP_GET;
    info.handler = handleInfo;
    info.user_ctx = this;
    httpd_register_uri_handler(handle_, &info);

    httpd_uri_t factoryReset = {};
    factoryReset.uri = "/api/factory-reset";
    factoryReset.method = HTTP_POST;
    factoryReset.handler = handleFactoryReset;
    factoryReset.user_ctx = this;
    httpd_register_uri_handler(handle_, &factoryReset);

    // SPA fallback (must be last — wildcard)
    httpd_uri_t fallback = {};
    fallback.uri = "/*";
    fallback.method = HTTP_GET;
    fallback.handler = handleSpaFallback;
    fallback.user_ctx = this;
    httpd_register_uri_handler(handle_, &fallback);
}

// ---- Static Files ----

esp_err_t HttpServer::handleStaticFile(httpd_req_t* req)
{
    const char* uri = req->uri;
    const char* query = std::strchr(uri, '?');
    size_t uriLen = query ? static_cast<size_t>(query - uri) : std::strlen(uri);

    for (int i = 0; i < kWebAssetCount; i++)
    {
        if (std::strlen(kWebAssets[i].urlPath) == uriLen &&
            std::strncmp(kWebAssets[i].urlPath, uri, uriLen) == 0)
        {
            setCorsHeaders(req);
            httpd_resp_set_type(req, kWebAssets[i].mimeType);
            if (kWebAssets[i].isGzipped)
            {
                httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
            }
            const char* ext = std::strrchr(kWebAssets[i].urlPath, '.');
            if (ext && (std::strcmp(ext, ".js") == 0 || std::strcmp(ext, ".css") == 0))
            {
                httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000, immutable");
            }
            return httpd_resp_send(req, reinterpret_cast<const char*>(kWebAssets[i].data),
                                   kWebAssets[i].size);
        }
    }
    return handleSpaFallback(req);
}

esp_err_t HttpServer::handleSpaFallback(httpd_req_t* req)
{
    for (int i = 0; i < kWebAssetCount; i++)
    {
        if (std::strcmp(kWebAssets[i].urlPath, "/index.html") == 0)
        {
            httpd_resp_set_type(req, "text/html");
            if (kWebAssets[i].isGzipped)
            {
                httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
            }
            return httpd_resp_send(req, reinterpret_cast<const char*>(kWebAssets[i].data),
                                   kWebAssets[i].size);
        }
    }
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
    return ESP_OK;
}

// ---- API Handlers ----

esp_err_t HttpServer::handlePing(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: ping");
    setCorsHeaders(req);
    sendJsonOk(req);
    return ESP_OK;
}

esp_err_t HttpServer::handleRestart(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: restart");
    auto* server = static_cast<HttpServer*>(req->user_ctx);
    setCorsHeaders(req);
    sendJsonOk(req);
    server->scheduleReboot(kDefaultRebootDelayMs);
    return ESP_OK;
}

esp_err_t HttpServer::handleBootApp(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: boot into app");
    auto* server = static_cast<HttpServer*>(req->user_ctx);
    setCorsHeaders(req);

    const esp_partition_t* appPartition = nullptr;
    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    while (it != nullptr)
    {
        const esp_partition_t* p = esp_partition_get(it);
        if (strcmp(p->label, "safemode") != 0)
        {
            appPartition = p;
            break;
        }
        it = esp_partition_next(it);
    }
    if (it != nullptr)
    {
        esp_partition_iterator_release(it);
    }

    if (appPartition == nullptr)
    {
        ESP_LOGE(kTag, "No app partition found");
        sendJsonError(req, 500);
        return ESP_OK;
    }

    if (esp_ota_set_boot_partition(appPartition) != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to set boot partition");
        sendJsonError(req, 500);
        return ESP_OK;
    }

    sendJsonOk(req);
    server->scheduleReboot(kDefaultRebootDelayMs);
    return ESP_OK;
}

esp_err_t HttpServer::handleUpdate(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: OTA update (%d bytes)", req->content_len);
    auto* server = static_cast<HttpServer*>(req->user_ctx);
    setCorsHeaders(req);

    if (server->ota_ == nullptr)
    {
        ESP_LOGE(kTag, "OTA updater not configured");
        sendJsonError(req, 500);
        return ESP_OK;
    }

    esp_err_t ret = server->ota_->begin();
    if (ret != ESP_OK)
    {
        sendJsonError(req, 500);
        return ESP_OK;
    }

    char buf[1024];
    int remaining = req->content_len;
    int received = 0;

    while (remaining > 0)
    {
        int toRead = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
        int read = httpd_req_recv(req, buf, toRead);
        if (read <= 0)
        {
            if (read == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            ESP_LOGE(kTag, "OTA receive error");
            server->ota_->abort();
            sendJsonError(req, 500);
            return ESP_OK;
        }

        ret = server->ota_->write(buf, read);
        if (ret != ESP_OK)
        {
            sendJsonError(req, 500);
            return ESP_OK;
        }

        received += read;
        remaining -= read;
    }

    ESP_LOGI(kTag, "OTA: received %d bytes, finishing...", received);

    ret = server->ota_->finish();
    if (ret != ESP_OK)
    {
        sendJsonError(req, 500);
        return ESP_OK;
    }

    sendJsonOk(req);
    server->scheduleReboot(kDefaultRebootDelayMs);
    return ESP_OK;
}

esp_err_t HttpServer::handleInfo(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: info");
    auto* server = static_cast<HttpServer*>(req->user_ctx);
    setCorsHeaders(req);

    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);

    const esp_app_desc_t* appDesc = esp_app_get_description();
    const esp_partition_t* running = esp_ota_get_running_partition();

    const char* appLabel = "none";
    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    while (it != nullptr)
    {
        const esp_partition_t* p = esp_partition_get(it);
        if (strcmp(p->label, "safemode") != 0)
        {
            appLabel = p->label;
            break;
        }
        it = esp_partition_next(it);
    }
    if (it != nullptr)
    {
        esp_partition_iterator_release(it);
    }

    bool factoryResetEnabled = server->nvsAvailable_ && safemode::isFactoryResetEnabled();

    char json[512];
    int len = snprintf(json, sizeof(json),
        "{"
        "\"chip\":\"%s\","
        "\"revision\":%d,"
        "\"cores\":%d,"
        "\"idfVersion\":\"%s\","
        "\"freeHeap\":%lu,"
        "\"runningPartition\":\"%s\","
        "\"appPartition\":\"%s\","
        "\"firmwareVersion\":\"%s\","
        "\"factoryResetEnabled\":%s"
        "}",
        CONFIG_IDF_TARGET,
        chipInfo.revision,
        chipInfo.cores,
        esp_get_idf_version(),
        esp_get_free_heap_size(),
        running ? running->label : "unknown",
        appLabel,
        appDesc ? appDesc->version : "unknown",
        factoryResetEnabled ? "true" : "false"
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, len);
    return ESP_OK;
}

esp_err_t HttpServer::handleFactoryReset(httpd_req_t* req)
{
    ESP_LOGI(kTag, "API: factory reset");
    auto* server = static_cast<HttpServer*>(req->user_ctx);
    setCorsHeaders(req);

    if (!server->nvsAvailable_ || !safemode::isFactoryResetEnabled())
    {
        ESP_LOGW(kTag, "Factory reset not available");
        sendJsonError(req, 400);
        return ESP_OK;
    }

    // Send response first, then perform the reset in a dedicated task.
    // We can't erase NVS while WiFi is running (PHY task accesses NVS
    // calibration data), so the task stops WiFi, erases, and reboots.
    sendJsonOk(req);

    xTaskCreate(factoryResetTask, "factory_reset", 8192, nullptr, 5, nullptr);

    return ESP_OK;
}

}  // namespace safemode
