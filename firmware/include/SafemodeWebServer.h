
#ifndef SAFEMODE_WEBSERVER_H
#define SAFEMODE_WEBSERVER_H

#include <WiFi.h>
#include <aWOT.h>
#include <array>

#define SAFEMODE_WEBSERVER_MAX_CLIENTS 16
#define SAFEMODE_WEBSERVER_MAX_IDLE 5000

class SafemodeWebServer {
public:
    SafemodeWebServer(int port = 80);
    ~SafemodeWebServer();

    void setup();
    void loop();

private:
    WiFiServer wifiServer;
    Application webServer;
    std::array<WiFiClient, SAFEMODE_WEBSERVER_MAX_CLIENTS> clients;
    std::array<long, SAFEMODE_WEBSERVER_MAX_CLIENTS> timeToLive;
};

extern SafemodeWebServer SAFEMODE_WEBSERVER;

#endif
