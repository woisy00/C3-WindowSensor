#pragma once

#include <ArduinoHA.h>
#include <WiFiClient.h>
#include "Config.h"
#include "BatteryUtil.h"
#include "WindowSensorDefines.h"

class HAManager {
public:
    explicit HAManager(Config& cfg);

    void begin();
    void loop();

    void setWindowState(bool open);
    void setBatteryState(float voltage);

private:
    Config& _config;

    WiFiClient _wifiClient;
    HADevice _haDevice;
    HAMqtt _mqtt;

    HABinarySensor _windowSensor;
    HASensorNumber _batterySensor;
    HASensorNumber _batteryPercent;
    HASensor _firmwareVersion;

    String _firmwareUpdateTopic;

    bool _firstLoop = true;
    bool _windowState;
    float _batteryVoltage;
    String _configUrl;

    static HAManager* _instance;

    static void onMessage(const char* topic, const uint8_t* payload, uint16_t length);
};