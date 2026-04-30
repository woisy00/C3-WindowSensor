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
      _mqtt(_wifiClient, _haDevice),
      _firmwareVersion("firmware", HASensor::JsonAttributesFeature),
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
    _haDevice.setModel(HW_MODEL);
    _haDevice.setManufacturer(HW_MANUFACTURER);    

    _haDevice.enableSharedAvailability();
    _haDevice.enableExtendedUniqueIds();
    
    // Create unique IDs
    _firmwareUpdateTopic = String("aha/sensor/") + _haDevice.getUniqueId() + "/firmware/update";

    // Firmware version sensor
    _firmwareVersion.setName("Firmware version");
    //_firmwareVersion->setDeviceClass("firmware");
    _mqtt.addDeviceType(&_firmwareVersion);
    
    // Window sensor
    _windowSensor.setDeviceClass("window");
    _windowSensor.setName("Window Sensor");
    _mqtt.addDeviceType(&_windowSensor);

    // Battery voltage
    _batterySensor.setName("Battery voltage");
    _batterySensor.setDeviceClass("voltage");
    _batterySensor.setUnitOfMeasurement("V");    
    _mqtt.addDeviceType(&_batterySensor);

    // Battery %
    _batteryPercent.setName("Battery");
    _batteryPercent.setDeviceClass("battery");
    _batteryPercent.setUnitOfMeasurement("%");
    _mqtt.addDeviceType(&_batteryPercent);


    _mqtt.begin(
        _config.mqttHost.c_str(),
        _config.mqttUser.c_str(),
        _config.mqttPass.c_str()
    );

    
    // Subscribe to firmware update topic using device unique ID
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
    if (!_mqtt.isConnected()) {
        return;
    }
    if (_firstLoop) {
        String firmwareAttrs = "{\"device_name\": \"" + _config.deviceName + "\", \"firmware_sensor\": \"" + _haDevice.getUniqueId() + "_" + _firmwareVersion.uniqueId() + "\",  \"model\": \"" + String(HW_MODEL) + "\", \"manufacturer\": \"" + String(HW_MANUFACTURER) + "\", \"update\": \"" + _firmwareUpdateTopic + "\"}";
        _firmwareVersion.setJsonAttributes(firmwareAttrs.c_str());
        _firmwareVersion.setValue(VERSION);
        _firstLoop = false;
    }
    if (_windowSensor.getCurrentState() != _windowState) {
        _windowSensor.setState(_windowState, false);
    }

    if (_batterySensor.getCurrentValue().toFloat() != _batteryVoltage) {
        _batterySensor.setValue(_batteryVoltage, false);
        // Default: alkaline (can be made configurable later)
        float percent = BatteryUtil::voltageToPercent(_batteryVoltage, BatteryType::ALKALINE);
        _batteryPercent.setValue(percent, false);
    }
    _mqtt.loop();
}

// --- Publishers ---

void HAManager::setWindowState(bool open) {
    _windowState = open;
}

void HAManager::setBatteryState(float voltage) {
    _batteryVoltage = voltage;
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