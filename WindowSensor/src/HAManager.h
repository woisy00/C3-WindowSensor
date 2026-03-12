#pragma once

#include <ArduinoHA.h>
#include <WiFiClient.h>
#include "Config.h"

class HAManager {
public:
    explicit HAManager(Config& cfg);
    void begin();
    void loop();
    void publishWindow(bool open, bool force = true);
    void publishBattery(float voltage, bool force = true);

private:
    Config& _config;
    WiFiClient _wifiClient;
    HADevice _haDevice;
    HAMqtt _mqtt;
    HABinarySensor _windowSensor;
    HASensorNumber _batterySensor;

    static HAManager* _instance;
};
