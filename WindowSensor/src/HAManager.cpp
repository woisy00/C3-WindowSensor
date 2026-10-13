#include "HAManager.h"
#include "ButtonMonitor.h"

#define FW_OBJECT_ID "firmware"

// --- Constructor ---

HAManager::HAManager(Config& cfg)
    : _config(cfg),
      _haDevice(),
      _mqtt(_wifiClient, _haDevice),
      _windowSensor("window"),
      _batterySensor("battery_voltage", HASensorNumber::PrecisionP2),
      _batteryPercent("battery_percent", HASensorNumber::PrecisionP0) {
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
    static HAManager* self = this;  // capture 'this' pointer for use in lambda
    _mqtt.onMessage([](const char* topic, const uint8_t* payload, uint16_t length) {
        String message;
        for (uint16_t i = 0; i < length; i++) {
            message += (char)payload[i];
        }
        DEBUG_PRINTLN("Firmware update request received on topic: " + String(topic));
        DEBUG_PRINTLN("Message: " + message);

        self->_performUpdate(message);
    });
}

// --- Loop ---

Hardware::WindowState HAManager::work() {
    while (!_mqtt.isConnected()) {
        _mqtt.loop();
        delay(10);        
        // allow entering maintenance when the broker is unreachable
        if (ButtonMonitor::service()) {
            return Hardware::readWindowState();
        }
    }

    _mqtt.subscribe(_firmwareUpdateTopic.c_str());
    DEBUG_PRINTLN("Subscribed to firmware update topic (after connect): " + _firmwareUpdateTopic);

    Hardware::WindowState windowState = Hardware::readWindowState();
    float batteryVoltage = Hardware::readBattery(_config.rTop, _config.rBottom);

    _setFirmwareVersion();
    _windowSensor.setState(windowState.isOpen(), false);

    #ifdef DEBUG
        JsonDocument debugInfo;
        debugInfo["pin_no"] = windowState.no;
        debugInfo["pin_nc"] = windowState.nc;

        _publish("debug", debugInfo, false);
    #endif

    _batterySensor.setValue(batteryVoltage, false);
    // Default: alkaline (can be made configurable later)
    float percent = BatteryUtil::voltageToPercent(batteryVoltage, BatteryType::ALKALINE);
    _batteryPercent.setValue(percent, false);

    unsigned long start = millis();
    while (millis() - start < 1500 || _updatingFirmware) {  // usually 100–300ms is enough
        _mqtt.loop();
        delay(10);
        if (!_updatingFirmware && ButtonMonitor::service()) {
            break;
        }
    }

    return windowState;
}

void HAManager::_setFirmwareVersion(const char* latestVersion) {

    const uint16_t topicLength = HASerializer::calculateDataTopicLength(
        FW_OBJECT_ID,
        AHATOFSTR(HAStateTopic)
    );
    if (topicLength == 0) {
        return;
    }

    char fullTopic[topicLength];
    if (!HASerializer::generateDataTopic(
        fullTopic,
        FW_OBJECT_ID,
        AHATOFSTR(HAStateTopic)
    )) {
        return;
    }

    JsonDocument payload;
    payload["installed_version"] = VERSION;
    if (latestVersion){
        payload["latest_version"] = latestVersion;
    }
    payload["in_progress"] = _updatingFirmware;
    payload["device_id"] = _haDevice.getUniqueId();
    payload["device_name"] = _config.deviceName;
    payload["model"] = HW_MODEL;
    payload["manufacturer"] = HW_MANUFACTURER;
    payload["update_topic"] = _firmwareUpdateTopic;

    _publish(fullTopic, payload, true);
}

// --- Publishers ---
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

    String version = doc["version"] | "unknown";
    String currentVersion = String(VERSION);
    DEBUG_PRINTLN("Requested firmware version: " + version);
    DEBUG_PRINTLN("Current firmware version: " + currentVersion);

    if (status == "update_started" && version == currentVersion) {
        DEBUG_PRINTLN("Update already done");
        // finally "delete" the update request
        _mqtt.publish(_firmwareUpdateTopic.c_str(), "", true);
        return;
    }

    if (doc["url"]) {
        _updatingFirmware = true;

        String firmwareUrl = doc["url"];        
        DEBUG_PRINTLN("Starting firmware update to " + version + " from: " + firmwareUrl);
        _setFirmwareVersion(version.c_str());

        // confirm update initiation
        JsonDocument confirmPayload;
        confirmPayload["url"] = firmwareUrl;
        confirmPayload["version"] = version;
        confirmPayload["status"] = "update_started";          
        _publish(_firmwareUpdateTopic.c_str(), confirmPayload, true);

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
            _updatingFirmware = false;
            ESP.restart();
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
    _mqtt.loop();
    _mqtt.loop();
    DEBUG_PRINTLN("Published messageto topic: " + String(topic) + ", retained: " + String(retained));
    DEBUG_PRINTLN("Payload: " + payload.as<String>());    
}