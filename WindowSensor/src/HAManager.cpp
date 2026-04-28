#include "HAManager.h"
#include "BatteryUtil.h"
#include "version.h"
#include "Utils.h"
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

HAManager* HAManager::_instance = nullptr;

// --- Constructor ---

HAManager::HAManager(Config& cfg)
    : _config(cfg),
      _haDevice(),
      _mqtt(_wifiClient, _haDevice),
      _windowSensor("window"),
      _batterySensor("battery_voltage", HASensorNumber::PrecisionP2),
      _batteryPercent("battery_percent", HASensorNumber::PrecisionP0) {
    _instance = this;
}

// --- Setup ---

void HAManager::begin() {
    byte mac[6];
    WiFi.macAddress(mac);

    _haDevice.setUniqueId(mac, sizeof(mac));
    _haDevice.setName(_config.deviceName.c_str());
    _haDevice.setSoftwareVersion(VERSION);
    _haDevice.setModel("BWS-01");
    _haDevice.setManufacturer("bergermeier");

    _haDevice.enableSharedAvailability();

    // Window sensor
    _windowSensor.setDeviceClass("window");
    _windowSensor.setName("Window Sensor");

    // Battery voltage
    _batterySensor.setName("Battery voltage");
    _batterySensor.setDeviceClass("voltage");
    _batterySensor.setUnitOfMeasurement("V");

    // Battery %
    _batteryPercent.setName("Battery");
    _batteryPercent.setDeviceClass("battery");
    _batteryPercent.setUnitOfMeasurement("%");


    _mqtt.begin(
        _config.mqttHost.c_str(),
        _config.mqttUser.c_str(),
        _config.mqttPass.c_str()
    );

    // Subscribe to firmware update topic using device unique ID
    String topic = String("homeassistant/sensor/") + _haDevice.getUniqueId() + "/firmware/update";
    _mqtt.subscribe(topic.c_str());
    _mqtt.onMessage(onMessage);
}

// --- Loop ---

void HAManager::loop() {
    _mqtt.loop();
}

// --- Publishers ---

void HAManager::publishWindow(bool open, bool force) {
    _windowSensor.setState(open, force);
}

void HAManager::publishBattery(float voltage, bool force) {
    _batterySensor.setValue(voltage, force);

    // Default: alkaline (can be made configurable later)
    float percent = BatteryUtil::voltageToPercent(voltage, BatteryType::ALKALINE);

    _batteryPercent.setValue(percent, force);
}

// --- MQTT Callbacks ---

void HAManager::onMessage(const char* topic, const uint8_t* payload, uint16_t length) {
    if (_instance) {
        String message;
        for (uint16_t i = 0; i < length; i++) {
            message += (char)payload[i];
        }
        Utils::logMsg("Firmware update request received on topic: " + String(topic));
        Utils::logMsg("Message: " + message);

        // Parse JSON message
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        if (error) {
            Utils::logMsg("Failed to parse JSON: " + String(error.c_str()));
            return;
        }

        if (doc["url"]) {
            String firmwareUrl = doc["url"];
            Utils::logMsg("Starting firmware update from: " + firmwareUrl);
            WiFiClient client;
            HTTPUpdate httpUpdate;
            HTTPUpdateResult result = httpUpdate.update(client, firmwareUrl);
            if (result == HTTP_UPDATE_OK) {
                Utils::logMsg("Firmware update successful. Restarting...");
                ESP.restart();
            } else {
                Utils::logMsg("Firmware update failed. Error: " + String(httpUpdate.getLastError()) + " - " + httpUpdate.getLastErrorString());
            }
        } else {
            Utils::logMsg("JSON message missing 'url' field");
        }
    }
}