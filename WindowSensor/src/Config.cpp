#include "WindowSensorDefines.h"
#include "Config.h"
#include "Utils.h"

Config::Config() : deviceName(""),
      mqttHost(""),
      mqttUser(""),
      mqttPass(""),
      sleepTime(4UL * 60UL * 60UL * 1000UL),  // 4h default
      loopTime(3000UL),
      rTop(1000.0),
      rBottom(470.0) {
}

bool Config::load() {
    if (_prefs.begin("config", true)) {
        deviceName = _prefs.getString("dev", "");
        if (deviceName.length() == 0) {
            deviceName = "window-sensor";            
        } else {
            mqttHost = _prefs.getString("mqtt_host", "");
            mqttUser = _prefs.getString("mqtt_user", "");
            mqttPass = _prefs.getString("mqtt_pass", "");
            sleepTime = _prefs.getULong("sleep_time", sleepTime);
            loopTime = _prefs.getULong("loop_time", loopTime);
            rTop = _prefs.getFloat("rTop", rTop);
            rBottom = _prefs.getFloat("rBottom", rBottom);
        }
        _prefs.end();        
    } else {
        // Set fallback device name if NVS is empty
        deviceName = "window-sensor";
    }

    DEBUG_PRINTLN("Loaded config:");
    DEBUG_PRINTLN("  device_name: " + deviceName);
    DEBUG_PRINTLN("  mqtt_host: " + mqttHost);
    DEBUG_PRINTLN("  mqtt_user: " + mqttUser);
    DEBUG_PRINTLN("  mqtt_pass: " + mqttPass);
    DEBUG_PRINTLN("  sleep_time: " + Utils::millisToHuman(sleepTime));
    DEBUG_PRINTLN("  loop_time: " + Utils::millisToHuman(loopTime));
    DEBUG_PRINTLN("  rTop (kΩ): " + String(rTop));
    DEBUG_PRINTLN("  rBottom (kΩ): " + String(rBottom));

    return mqttHost.length() > 0;
}

void Config::save() {
    // update fields from portal parameters if they exist
    if (_p_dev->getValue() != nullptr) {
        deviceName = _p_dev->getValue();
        mqttHost = _p_host->getValue();
        mqttUser = _p_user->getValue();
        mqttPass = _p_pass->getValue();
        sleepTime = Utils::parseDuration(_p_sleep_time->getValue());
        loopTime = Utils::parseDuration(_p_loop_time->getValue());
        rTop = String(_p_rTop->getValue()).toFloat();
        rBottom = String(_p_rBottom->getValue()).toFloat();
    }

    _prefs.begin("config", false);
    _prefs.putString("dev", deviceName);
    _prefs.putString("mqtt_host", mqttHost);
    _prefs.putString("mqtt_user", mqttUser);
    _prefs.putString("mqtt_pass", mqttPass);
    _prefs.putULong("sleep_time", sleepTime);
    _prefs.putULong("loop_time", loopTime);
    _prefs.putFloat("rTop", rTop);
    _prefs.putFloat("rBottom", rBottom);
    _prefs.end();
}

void Config::configureWiFiManager(WiFiManager& wm) {

    // create group headers using custom HTML parameters
    _p_group_general = new WiFiManagerParameter("<h2>General</h2><hr>");
    _p_group_mqtt = new WiFiManagerParameter("<h2>MQTT</h2><hr>");
    // advanced section start/end tags
    _p_adv_begin = new WiFiManagerParameter("<details><summary>Advanced</summary>");
    _p_adv_end = new WiFiManagerParameter("</details>");

    _p_dev = new WiFiManagerParameter("dev", "Device Name", deviceName.c_str(), 32);
    _p_host = new WiFiManagerParameter("mqtt", "MQTT Host", mqttHost.c_str(), 64);
    _p_user = new WiFiManagerParameter("user", "MQTT User", mqttUser.c_str(), 32);
    _p_pass = new WiFiManagerParameter("pass", "MQTT Pass", mqttPass.c_str(), 32);
    _p_sleep_time = new WiFiManagerParameter("sleep_time", "Sleep time", Utils::millisToHuman(sleepTime).c_str(), 32);
    _p_loop_time = new WiFiManagerParameter("loop_time", "Loop time", Utils::millisToHuman(loopTime).c_str(), 32);
    // use number inputs so only digits/decimal allowed
    _p_rTop = new WiFiManagerParameter("rTop", "Divider Rₜₒₚ (kΩ)", String(rTop).c_str(), 16, "min=0 step=0.1 type=number");
    _p_rBottom = new WiFiManagerParameter("rBottom", "Divider R₆ₒₜₜₒₘ (kΩ)", String(rBottom).c_str(), 16, "min=0 step=0.1 type=number");

    // add them in grouped order
    wm.addParameter(_p_group_general);
    wm.addParameter(_p_dev);

    wm.addParameter(_p_group_mqtt);
    wm.addParameter(_p_host);
    wm.addParameter(_p_user);
    wm.addParameter(_p_pass);

    wm.addParameter(_p_adv_begin);
    wm.addParameter(_p_sleep_time);
    wm.addParameter(_p_loop_time);
    wm.addParameter(_p_rTop);
    wm.addParameter(_p_rBottom);
    wm.addParameter(_p_adv_end);

    wm.setConfigPortalTimeout(0);
    wm.setShowStaticFields(true);
    wm.setShowDnsFields(true);
}
