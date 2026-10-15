#include "WiFiConnector.h"
#include "FirmwareManager.h"
#include "Utils.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "WindowSensorDefines.h"

WiFiConnector* WiFiConnector::_instance = nullptr;

WiFiConnector::WiFiConnector(Config& cfg)
    : _config(cfg) {
    _instance = this;
}

void WiFiConnector::saveConfigCallback() {
    if (_instance) {
        _instance->_config.markSave();
    }
}
void WiFiConnector::connect(bool forceConfig, uint32_t portalTimeoutSec, FirmwareManager* firmwareManager) {
    if (!_wmPtr) {
        _wmPtr = new WiFiManager();
    }
    WiFiManager& _wm = *_wmPtr;
    // set hostname early so MDNS and WiFiManager see it
    WiFi.hostname(_config.deviceName);
    _wm.setHostname(_config.deviceName);

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);
    WiFi.setAutoReconnect(true);
    WiFi.begin();
    WiFi.hostname(_config.deviceName);
    _wm.setHostname(_config.deviceName);

    bool rebootRequested = false;

    if (MDNS.begin(_config.deviceName.c_str())) {
        DEBUG_PRINTLN("mDNS " + _config.deviceName + ".local ready");
    }

    if (WiFi.waitForConnectResult(8000) != WL_CONNECTED) {
        rebootRequested = true;
        DEBUG_PRINTLN("WiFi failed... starting captive portal");
        _config.configureWiFiManager(_wm);
        _wm.setSaveConfigCallback(WiFiConnector::saveConfigCallback);
        _wm.setSaveParamsCallback(WiFiConnector::saveConfigCallback);
        byte mac[6];
        WiFi.macAddress(mac);

        // Create default device name with MAC address suffix
        char macStr[13]; // "AABBCCDDEEFF"
        snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", 
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        String apName = "Window-Sensor-" + String(macStr).substring(6); // Last 4 chars of MAC
        if (portalTimeoutSec > 0) {
            _wm.setConfigPortalTimeout(portalTimeoutSec);
        }
        if (firmwareManager != nullptr) {
            firmwareManager->maintenancePortalStarted();
        }
        _wm.startConfigPortal(apName.c_str());
    } else if (forceConfig) {
        WiFi.hostname(_config.deviceName);
        _wm.setHostname(_config.deviceName);

        rebootRequested = true;
        _config.configureWiFiManager(_wm);
        _wm.setSaveConfigCallback(WiFiConnector::saveConfigCallback);
        _wm.setSaveParamsCallback(WiFiConnector::saveConfigCallback);
        if (firmwareManager != nullptr) {
            firmwareManager->maintenancePortalStarted();
        }
        _wm.startWebPortal();
        DEBUG_PRINTLN("Portal running");
        const unsigned long portalStart = millis();
        while (_wm.getWebPortalActive()) {
            _wm.process();
            if (firmwareManager != nullptr) {
                firmwareManager->service();
            }
            delay(100);
            if (portalTimeoutSec > 0 && millis() - portalStart > portalTimeoutSec * 1000UL) {
                DEBUG_PRINTLN("Portal timeout");
                _wm.stopWebPortal();
                break;
            }
        }
        DEBUG_PRINTLN("Exited portal");
    } else {
        WiFi.hostname(_config.deviceName);
        _wm.setHostname(_config.deviceName);
    }

    if (_config.shouldSave()) {
        _config.save();
        delay(1000);
    }
    if (forceConfig && firmwareManager != nullptr) {
        firmwareManager->service();
    }

    if (rebootRequested) {
        DEBUG_PRINTLN("Rebooting after WiFiManager exited");
        ESP.restart();
    }
}
