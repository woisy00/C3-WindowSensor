#include "FirmwareManager.h"
#include "FirmwareStrings.h"
#include "Hardware.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "Utils.h"
#include "WindowSensorDefines.h"
#include "version.h"

namespace Strings = FirmwareStrings;

FirmwareManager* FirmwareManager::_instance = nullptr;

FirmwareManager::FirmwareManager(Config& cfg)
    : _config(cfg),
      _mqtt(_wifiClient) {
    _instance = this;
}

void FirmwareManager::begin() {
    _deviceId = Utils::BT_MAC(Strings::Format::DeviceId);
    _updateTopic = Strings::Mqtt::UpdateTopic(_deviceId);
    _stateTopic = Strings::Mqtt::StateTopic(_deviceId);
    _discoveryTopic = Strings::Mqtt::DiscoveryTopic(_deviceId);
    _mqtt.setBufferSize(1024);
    _mqtt.setCallback(FirmwareManager::_messageCallback);
}

void FirmwareManager::maintenancePortalStarted() {
    _discoveryPending = true;
    _versionCheckPending = true;
    service();
}

void FirmwareManager::service() {
    if (WiFi.status() != WL_CONNECTED || _config.mqttHost.isEmpty()) {
        return;
    }

    if (!_mqtt.connected() && !_connect()) {
        return;
    }

    if (_discoveryPending) {
        _discoveryPending = false;
        _publishDiscovery(VERSION);
        _publishState(VERSION);
    }
    _mqtt.loop();
}

bool FirmwareManager::_connect() {
    const unsigned long now = millis();
    if (now - _lastConnectAttemptMs < 5000UL) {
        return false;
    }
    _lastConnectAttemptMs = now;
    _mqtt.setServer(_config.mqttHost.c_str(), 1883);

    const String clientId = String(Strings::Mqtt::ClientIdPrefix) + _deviceId.substring(8);
    bool connected;
    if (_config.mqttUser.isEmpty()) {
        connected = _mqtt.connect(clientId.c_str());
    } else {
        connected = _mqtt.connect(clientId.c_str(), _config.mqttUser.c_str(), _config.mqttPass.c_str());
    }
    if (connected) {
        DEBUG_PRINTLN(Strings::Log::MqttConnected);
        _mqtt.subscribe(_updateTopic.c_str());
    } else {
        DEBUG_PRINTLN(Strings::Log::MqttConnectionFailed + String(_mqtt.state()));
    }
    return connected;
}

void FirmwareManager::_messageCallback(char* topic, uint8_t* payload, unsigned int length) {
    if (_instance == nullptr || _instance->_updateTopic != topic) {
        return;
    }
    JsonDocument command;
    const DeserializationError error = deserializeJson(command, reinterpret_cast<const char*>(payload), length);
    if (error || !command[Strings::Json::Url].is<const char*>() ||
        !command[Strings::Json::Version].is<const char*>()) {
        DEBUG_PRINTLN(Strings::Log::UpdatePayloadInvalid);
        return;
    }

    const String firmwareUrl = command[Strings::Json::Url].as<String>();
    const String version = command[Strings::Json::Version].as<String>();
    if (version.isEmpty() ||
        (!firmwareUrl.startsWith(Strings::Http::PlainUrlPrefix) &&
         !firmwareUrl.startsWith(Strings::Http::SecureUrlPrefix)) ||
        firmwareUrl.length() <= (firmwareUrl.startsWith(Strings::Http::SecureUrlPrefix)
                                 ? sizeof(Strings::Http::SecureUrlPrefix) - 1
                                 : sizeof(Strings::Http::PlainUrlPrefix) - 1)) {
        DEBUG_PRINTLN(Strings::Log::UpdatePayloadInvalid);
        return;
    }

    _instance->_latestVersion = version;
    _instance->_performUpdate(firmwareUrl);
}

bool FirmwareManager::_publish(const String& topic, const String& payload) {
    const bool published = _mqtt.publish(topic.c_str(), payload.c_str(), true);
    DEBUG_PRINTLN(Strings::Log::PublishTopic + topic + Strings::Log::PublishResult +
                  String(published ? Strings::Log::PublishSuccess : Strings::Log::PublishFailed));
    DEBUG_PRINTLN(Strings::Log::PublishPayload + payload);
    return published;
}

void FirmwareManager::_publishDiscovery(const String& version) {
    JsonDocument payload;
    payload[Strings::Json::Name] = Strings::Mqtt::EntityName;
    payload[Strings::Json::UniqueId] = String(Strings::Mqtt::UniqueIdPrefix) + _deviceId;
    payload[Strings::Json::CommandTopic] = _updateTopic;
    payload[Strings::Json::PayloadInstall] = Strings::Mqtt::InstallPayload;
    payload[Strings::Json::StateTopic] = _stateTopic;

    payload[Strings::Json::LatestVersionTopic] = Strings::Mqtt::LatestVersionTopic;
    payload[Strings::Json::LatestVersionTemplate] = Strings::Mqtt::LatestVersionTemplate;

    JsonObject device = payload[Strings::Json::Device].to<JsonObject>();
    device[Strings::Json::Name] = _config.deviceName;
    device[Strings::Json::ConfigurationUrl] = String(Strings::Http::PlainUrlPrefix) + WiFi.localIP().toString();
    device[Strings::Json::Model] = HW_MODEL;
    device[Strings::Json::Manufacturer] = HW_MANUFACTURER;
    device[Strings::Json::Identifiers] = _deviceId;
    
    JsonArray connections = device[Strings::Json::Connections].to<JsonArray>();
    JsonArray wifiConnection = connections.add<JsonArray>();
    wifiConnection.add(Strings::Mqtt::MacConnection);
    wifiConnection.add(Utils::WIFI_MAC(Strings::Format::MacAddress));
    JsonArray bluetoothConnection = connections.add<JsonArray>();
    bluetoothConnection.add(Strings::Mqtt::BluetoothConnection);
    bluetoothConnection.add(Utils::BT_MAC(Strings::Format::MacAddress));
    device[Strings::Json::SoftwareVersion] = version.isEmpty() ? VERSION : version;

    String serialized;
    serializeJson(payload, serialized);
    _publish(_discoveryTopic, serialized);
}

void FirmwareManager::_publishState(const String& version, int progress) {
    JsonDocument payload;
    payload[Strings::Json::InstalledVersion] = version;
    payload[Strings::Json::InProgress] = _updateInProgress;
    if (progress >= 0 && progress <= 100) {
        payload[Strings::Json::UpdatePercentage] = progress;
    } else {
        payload[Strings::Json::UpdatePercentage] = nullptr;
    }
    const auto batteryReading = Hardware::readBattery(_config.rTop, _config.rBottom, _config.batteryCorrectionFactor);
    JsonObject battery = payload[Strings::Json::Debug][Strings::Json::Battery].to<JsonObject>();
    JsonArray samples = battery[Strings::Json::Samples].to<JsonArray>();
    for (const auto sample : batteryReading.samples) {
        samples.add(sample);
    }
    battery[Strings::Json::DiscardedRaw] = batteryReading.discardedRaw;
    battery[Strings::Json::Raw] = batteryReading.raw;
    battery[Strings::Json::AdcMillivolts] = batteryReading.adcMillivolts;
    battery[Strings::Json::RTop] = batteryReading.rTop;
    battery[Strings::Json::RBottom] = batteryReading.rBottom;
    battery[Strings::Json::RBottomFallbackUsed] = batteryReading.rBottomFallbackUsed;
    battery[Strings::Json::DividerRatio] = batteryReading.dividerRatio;
    battery[Strings::Json::CorrectionFactor] = batteryReading.correctionFactor;
    battery[Strings::Json::BatteryVoltage] = batteryReading.batteryVoltage;
    String serialized;
    serializeJson(payload, serialized);
    _publish(_stateTopic, serialized);
}

void FirmwareManager::_performUpdate(const String& firmwareUrl) {
    _firmwareUrl = firmwareUrl;
    DEBUG_PRINTLN(Strings::Log::UpdateStarting + _firmwareUrl);
    _updateInProgress = true;
    _updateProgress = 0;
    _publishState(VERSION, 0);

    HTTPUpdate httpUpdate;
    httpUpdate.rebootOnUpdate(false);
    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpUpdate.onProgress([this](int current, int total) {
        if (current < 0 || total <= 0) {
            return;
        }
        const int completed = current > total ? total : current;
        const int percentage = static_cast<int>(static_cast<uint64_t>(completed) * 100 / total);
        const int progress = (percentage / 5) * 5;
        if (progress <= _updateProgress) {
            return;
        }
        _updateProgress = progress;
        _publishState(VERSION, progress);
    });
    HTTPUpdateResult result;
    if (_firmwareUrl.startsWith(Strings::Http::SecureUrlPrefix)) {
        WiFiClientSecure secureClient;
        secureClient.setInsecure();
        result = httpUpdate.update(secureClient, _firmwareUrl);
    } else {
        WiFiClient client;
        result = httpUpdate.update(client, _firmwareUrl);
    }
    if (result == HTTP_UPDATE_OK) {
        _updateInProgress = false;
        const String updatedVersion = _latestVersion.isEmpty() ? VERSION : _latestVersion;

        DEBUG_PRINTLN("Cleaning update request");
        _publish(_updateTopic, String());
                
        _publishDiscovery(updatedVersion);
        _publishState(updatedVersion);
        delay(100);
        DEBUG_PRINTLN(Strings::Log::UpdateSucceeded);
        ESP.restart();
        return;
    }

    const String errorText = String(httpUpdate.getLastError()) + Strings::Format::ErrorSeparator + httpUpdate.getLastErrorString();
    DEBUG_PRINTLN(Strings::Log::UpdateFailed + errorText);
    _updateInProgress = false;
    _publishState(VERSION);
}