#pragma once

#include <WiFiManager.h>
#include "Config.h"

class WiFiConnector {
public:
    explicit WiFiConnector(Config& cfg);
    void connect(bool forceConfig);

private:
    Config& _config;
    WiFiManager _wm;
    static WiFiConnector* _instance;
    static void saveConfigCallback();
};
