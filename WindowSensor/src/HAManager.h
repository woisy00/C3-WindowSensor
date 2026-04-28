#pragma once

#include <ArduinoHA.h>
#include <WiFiClient.h>
#include "Config.h"
#include "BatteryUtil.h"

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
    HASensorNumber _batteryPercent;

    static HAManager* _instance;

    static void onMessage(const char* topic, const uint8_t* payload, uint16_t length);
};