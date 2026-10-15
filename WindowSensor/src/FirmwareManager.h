#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include "Config.h"

class FirmwareManager {
public:
    explicit FirmwareManager(Config& cfg);
    void begin();
    void maintenancePortalStarted();
    void service();

private:
    Config& _config;
    WiFiClient _wifiClient;
    PubSubClient _mqtt;
    String _deviceId;
    String _updateTopic;
    String _discoveryTopic;
    String _stateTopic;
    String _latestVersion;
    String _firmwareUrl;
    bool _discoveryPending = false;
    bool _versionCheckPending = false;
    bool _updateInProgress = false;
    int _updateProgress = 0;
    unsigned long _lastConnectAttemptMs = 0;

    bool _connect();
    bool _publish(const String& topic, const String& payload);
    void _publishDiscovery(const String& version);
    void _publishState(const String& version, int progress = -1);
    void _performUpdate(const String& firmwareUrl);
    static void _messageCallback(char* topic, uint8_t* payload, unsigned int length);
    static FirmwareManager* _instance;
};