#include "HAManager.h"


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
    
    // set configuration url
    _configUrl = String("http://") + WiFi.localIP().toString();
    _haDevice.setConfigurationUrl(_configUrl.c_str());

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

        _instance->_performUpdate(message);
    }
}

void HAManager::_performUpdate(const String& message) {
    // Parse JSON message
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        DEBUG_PRINTLN("Failed to parse JSON: " + String(error.c_str()));
        return;
    }
    
    String status = doc["status"];
    if (status != "requested" && status != "update_started") {
        DEBUG_PRINTLN("Invalid command in update message");
        // delete message
        _mqtt.publish(_firmwareUpdateTopic.c_str(), "", true);
        return;
    }

    if (status == "requested" && doc["version"] == VERSION) {
        DEBUG_PRINTLN("Update already done");
        // finally "delete" the update request
        _mqtt.publish(_firmwareUpdateTopic.c_str(), "", true);
        return;
    }

    if (doc["url"]) {
        String firmwareUrl = doc["url"];
        String version = doc["version"] | "unknown";
        DEBUG_PRINTLN("Starting firmware update to " + version + " from: " + firmwareUrl);

        JsonDocument payload;
        payload["installed_version"] = VERSION;
        payload["latest_version"] = version;
        payload["title"] = "Firmware update to " + version;
        payload["in_progress"] = true;

        String payloadStr;
        serializeJson(payload, payloadStr);
        _firmwareVersion.setForceUpdate(true);
        _firmwareVersion.setValue(payloadStr.c_str());
        _firmwareVersion.setForceUpdate(false);

        // confirm update initiation
        JsonDocument confirmPayload;
        confirmPayload["url"] = firmwareUrl;
        confirmPayload["version"] = version;
        confirmPayload["status"] = "update_started";          
        _publish(_firmwareUpdateTopic.c_str(), confirmPayload);

        WiFiClient client;  // or WiFiClientSecure for HTTPS        
        HTTPUpdate httpUpdate;                
        
        HTTPUpdateResult result = httpUpdate.update(client, firmwareUrl);
        if (result == HTTP_UPDATE_OK) {
            DEBUG_PRINTLN("Firmware update successful. Restarting...");
            ESP.restart();
        } else {
            DEBUG_PRINTLN("Firmware update failed. Error: " + String(httpUpdate.getLastError()) + " - " + httpUpdate.getLastErrorString());
            JsonDocument payload;
            payload["error"] = "Firmware update failed: " + String(httpUpdate.getLastError()) + " - " + httpUpdate.getLastErrorString();
            _publish(_firmwareUpdateTopic.c_str(), payload); 
            
        }
    } else {
        DEBUG_PRINTLN("JSON message missing 'url' field");

        JsonDocument payload;
        payload["error"] = "Missing 'url' field in update message";        
        _publish(_firmwareUpdateTopic.c_str(), payload);
    }
}

void HAManager::_publish(const String& topic, const JsonDocument& payload, bool retained) { 
    String payloadStr;
    serializeJson(payload, payloadStr);
    _mqtt.publish(topic.c_str(), payloadStr.c_str(), retained);
}