#include "HAManager.h"
#include "BatteryUtil.h"
#include "version.h"
#include "Utils.h"
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include "WindowSensorDefines.h"
#include "UpdateManager.h"

HAManager* HAManager::_instance = nullptr;

// --- Constructor ---

HAManager::HAManager(Config& cfg)
    : _config(cfg),
      _haDevice(),
      _mqtt(_wifiClient, _haDevice) {
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
    
    // Create unique IDs
    _windowUniqueId = _config.deviceName + "_window";
    _batteryVoltageUniqueId = _config.deviceName + "_battery_voltage";
    _batteryPercentUniqueId = _config.deviceName + "_battery_percent";

    String base = _config.deviceName.c_str();
    // Window sensor
    _windowSensor = new HABinarySensor(_windowUniqueId.c_str());
    _windowSensor->setDeviceClass("window");
    _windowSensor->setName("Window Sensor");
    _mqtt.addDeviceType(_windowSensor);
    DEBUG_PRINTLN("Created window sensor with unique ID: " + _windowUniqueId);

    // Battery voltage
    _batterySensor = new HASensorNumber(_batteryVoltageUniqueId.c_str(), HASensorNumber::PrecisionP2);
    _batterySensor->setName("Battery voltage");
    _batterySensor->setDeviceClass("voltage");
    _batterySensor->setUnitOfMeasurement("V");    
    _mqtt.addDeviceType(_batterySensor);
    DEBUG_PRINTLN("Created battery voltage sensor with unique ID: " + _batteryVoltageUniqueId);

    // Battery %
    _batteryPercent = new HASensorNumber(_batteryPercentUniqueId.c_str(), HASensorNumber::PrecisionP0);
    _batteryPercent->setName("Battery");
    _batteryPercent->setDeviceClass("battery");
    _batteryPercent->setUnitOfMeasurement("%");
    _mqtt.addDeviceType(_batteryPercent);
    DEBUG_PRINTLN("Created battery percentage sensor with unique ID: " + _batteryPercentUniqueId);


    _mqtt.begin(
        _config.mqttHost.c_str(),
        _config.mqttUser.c_str(),
        _config.mqttPass.c_str()
    );

    // Subscribe to firmware update topic using device unique ID
    _firmwareUpdateTopic = String("aha/sensor/") + _haDevice.getUniqueId() + "/firmware/update";
    //DEBUG_PRINTLN("Subscribing to firmware update topic: " + _firmwareUpdateTopic);
    _mqtt.onMessage(onMessage);
    _mqtt.onConnected([]() {
        _instance->_mqtt.subscribe(_instance->_firmwareUpdateTopic.c_str());
        DEBUG_PRINTLN("Subscribed to firmware update topic (after connect): " + _instance->_firmwareUpdateTopic);
    });
    //_mqtt.subscribe(_firmwareUpdateTopic.c_str());
    //DEBUG_PRINTLN("Subscribed to firmware update topic: " + _firmwareUpdateTopic);
}

// --- Loop ---

void HAManager::loop() {
    _mqtt.loop();
}

// --- Publishers ---

void HAManager::publishWindow(bool open, bool force) {
    _windowSensor->setState(open, force);
}

void HAManager::publishBattery(float voltage, bool force) {
    _batterySensor->setValue(voltage, force);

    // Default: alkaline (can be made configurable later)
    float percent = BatteryUtil::voltageToPercent(voltage, BatteryType::ALKALINE);

    _batteryPercent->setValue(percent, force);
}

void HAManager::onMessage(const char* topic, const uint8_t* payload, uint16_t length) {
    if (_instance) {
        String message;
        for (uint16_t i = 0; i < length; i++) {
            message += (char)payload[i];
        }
        DEBUG_PRINTLN("Firmware update request received on topic: " + String(topic));
        DEBUG_PRINTLN("Message: " + message);

        UpdateManager::performUpdate(message);
    }
}