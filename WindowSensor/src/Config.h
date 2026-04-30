#pragma once

#include <Preferences.h>
#include <WiFiManager.h>
#include <Arduino.h>

class Config {
public:
    String deviceName;
    String mqttHost;
    String mqttUser;
    String mqttPass;
    unsigned long sleepTime;
    unsigned long loopTime;
    float rTop;
    float rBottom;

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
    WiFiManagerParameter* _p_loop_time;
    WiFiManagerParameter* _p_rTop;
    WiFiManagerParameter* _p_rBottom;

    // grouping headers (custom HTML only)
    WiFiManagerParameter* _p_group_general;
    WiFiManagerParameter* _p_group_mqtt;
    // advanced section uses collapsible details tag
    WiFiManagerParameter* _p_adv_begin;
    WiFiManagerParameter* _p_adv_end;

    bool _shouldSave;
};
