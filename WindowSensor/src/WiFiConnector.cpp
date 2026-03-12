#include "WiFiConnector.h"
#include "Utils.h"
#include <WiFi.h>
#include <ESPmDNS.h>

WiFiConnector* WiFiConnector::_instance = nullptr;

WiFiConnector::WiFiConnector(Config& cfg)
    : _config(cfg), _wm() {
    _instance = this;
}

void WiFiConnector::saveConfigCallback() {
    if (_instance) {
        _instance->_config.markSave();
    }
}

void WiFiConnector::connect(bool forceConfig) {
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
#ifdef DEBUG
        Utils::logMsg("mDNS " + _config.deviceName + ".local ready");
#endif
    }

    if (WiFi.waitForConnectResult(8000) != WL_CONNECTED) {
        rebootRequested = true;
#ifdef DEBUG
        Utils::logMsg("WiFi failed... starting captive portal");
#endif
        _config.configureWiFiManager(_wm);
        _wm.setSaveConfigCallback(WiFiConnector::saveConfigCallback);
        _wm.startConfigPortal("Window-Sensor");
    } else if (forceConfig) {
        WiFi.hostname(_config.deviceName);
        _wm.setHostname(_config.deviceName);

        rebootRequested = true;
        _config.configureWiFiManager(_wm);
        _wm.setSaveConfigCallback(WiFiConnector::saveConfigCallback);
        _wm.startWebPortal();
#ifdef DEBUG
        Utils::logMsg("Portal running");
#endif
        while (_wm.getWebPortalActive()) {
            _wm.process();
            delay(100);
        }
#ifdef DEBUG
        Utils::logMsg("Exited portal");
#endif
    } else {
        WiFi.hostname(_config.deviceName);
        _wm.setHostname(_config.deviceName);
    }

    if (_config.shouldSave()) {
        _config.save();
        delay(1000);
    }

    if (rebootRequested) {
#ifdef DEBUG
        Utils::logMsg("Rebooting after WiFiManager exited");
#endif
        ESP.restart();
    }
}
