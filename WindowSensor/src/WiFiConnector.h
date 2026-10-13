#pragma once

#include <WiFiManager.h>
#include "Config.h"

class WiFiConnector {
public:
    explicit WiFiConnector(Config& cfg);
    // forceConfig: run the (blocking) portal even when Wi-Fi is connected.
    // portalTimeoutSec: 0 = no timeout (first provisioning), otherwise the portal is left after that long.
    void connect(bool forceConfig, uint32_t portalTimeoutSec = 0);

private:
    Config& _config;
    WiFiManager* _wmPtr = nullptr; // created lazily so Wi-Fi code is never touched on BLE wakes
    static WiFiConnector* _instance;
    static void saveConfigCallback();
};
