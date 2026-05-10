#pragma once

#include <ArduinoHA.h>
#include <WiFiClient.h>
#include "Config.h"
#include "BatteryUtil.h"
#include "WindowSensorDefines.h"
#include "BatteryUtil.h"
#include "Hardware.h"
#include "version.h"
#include "Utils.h"
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include "WindowSensorDefines.h"
#include <HADevice.h>
#include <utils/HASerializer.h>

class HAManager {
public:
    explicit HAManager(Config& cfg);
    void begin();
    Hardware::WindowState work();    

private:
    Config& _config;

    WiFiClient _wifiClient;
    HADevice _haDevice;
    HAMqtt _mqtt;

    HABinarySensor _windowSensor;
    HASensorNumber _batterySensor;
    HASensorNumber _batteryPercent;

    String _firmwareUpdateTopic;

    bool _updatingFirmware = false;
    String _configUrl;

    void _performUpdate(const String& message);
    void _publish(const String& topic, const JsonDocument& payload, bool retained = false);
    void _setFirmwareVersion(const char* latestVersion = nullptr);

};