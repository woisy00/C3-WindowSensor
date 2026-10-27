#pragma once

#include <Preferences.h>
#include <Arduino.h>
#include "ReedMode.h"

class Config {
public:
    String deviceName;
    String mqttHost;
    String mqttUser;
    String mqttPass;
    unsigned long sleepTime;
    float rTop;
    float rBottom;
    float batteryCorrectionFactor;
    bool blePortalScanEnabled;
    ReedMode reedMode;
    OpenLevel singleOpenLevel;
    String wifiSsid;
    String wifiPassword;
    bool wifiConfigured = false;
    bool staticEnabled = false;
    String staticIp;
    String gateway;
    String subnet;
    String dns;
    String portalPassword = "window-sensor";
    bool portalRequested = false;

    Config();

    bool load();
    bool save();
    bool setPortalRequested(bool requested);

private:
    Preferences _prefs;
};
