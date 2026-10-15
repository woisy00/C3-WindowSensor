#pragma once

#include <Preferences.h>
#include <WiFiManager.h>
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
    ReedMode reedMode;
    OpenLevel singleOpenLevel;

    Config();

    bool load();
    void save();
    void configureWiFiManager(WiFiManager& wm);
    bool shouldSave() const { return _shouldSave; }
    void markSave() { _shouldSave = true; }

private:
    Preferences _prefs;
    // visible input parameters
    WiFiManagerParameter* _p_dev;
    WiFiManagerParameter* _p_host;
    WiFiManagerParameter* _p_user;
    WiFiManagerParameter* _p_pass;
    WiFiManagerParameter* _p_sleep_time;
    WiFiManagerParameter* _p_rTop;
    WiFiManagerParameter* _p_rBottom;
    WiFiManagerParameter* _p_reed;
    WiFiManagerParameter* _p_open_level;

    // grouping headers (custom HTML only)
    WiFiManagerParameter* _p_group_general;
    WiFiManagerParameter* _p_group_mqtt;
    // advanced section uses collapsible details tag
    WiFiManagerParameter* _p_adv_begin;
    WiFiManagerParameter* _p_adv_end;

    bool _shouldSave;
};
