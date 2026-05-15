#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace safemode
{

class OtaUpdater;

class HttpServer
{
public:
    HttpServer() = default;
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void setOtaUpdater(OtaUpdater* ota) { ota_ = ota; }
    void setNvsAvailable(bool available) { nvsAvailable_ = available; }

    esp_err_t start(uint16_t port = 80);
    esp_err_t stop();

    bool isRunning() const { return handle_ != nullptr; }

    void scheduleReboot(uint32_t delayMs);

private:
    static constexpr const char* kTag = "http_server";
    static constexpr uint32_t kDefaultRebootDelayMs = 5000;

    httpd_handle_t handle_ = nullptr;
    OtaUpdater* ota_ = nullptr;
    bool nvsAvailable_ = false;

    void registerRoutes();

    static void setCorsHeaders(httpd_req_t* req);
    static esp_err_t handleOptions(httpd_req_t* req);

    static esp_err_t handleStaticFile(httpd_req_t* req);
    static esp_err_t handleSpaFallback(httpd_req_t* req);

    static esp_err_t handlePing(httpd_req_t* req);
    static esp_err_t handleRestart(httpd_req_t* req);
    static esp_err_t handleBootApp(httpd_req_t* req);
    static esp_err_t handleUpdateBegin(httpd_req_t* req);
    static esp_err_t handleUpdateChunk(httpd_req_t* req);
    static esp_err_t handleUpdateFinish(httpd_req_t* req);
    static esp_err_t handleUpdateAbort(httpd_req_t* req);
    static esp_err_t handleInfo(httpd_req_t* req);
    static esp_err_t handleFactoryReset(httpd_req_t* req);

    static void sendJsonOk(httpd_req_t* req);
    static void sendJsonError(httpd_req_t* req, int status);

    static void rebootTimerCallback(void* arg);
    static void factoryResetTask(void* arg);
};

}  // namespace safemode
