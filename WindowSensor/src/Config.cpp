#include "WindowSensorDefines.h"
#include "Config.h"
#include "Utils.h"

Config::Config() : deviceName(""),
      mqttHost(""),
      mqttUser(""),
      mqttPass(""),
      sleepTime(4UL * 60UL * 60UL * 1000UL),  // 4h default
      rTop(1000.0),
      rBottom(470.0),
      reportMode(ReportMode::MQTT),
      reedMode(ReedMode::SPDT),
      singleOpenLevel(OpenLevel::High),
      _p_dev(nullptr),
      _shouldSave(false) {
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
            rTop = _prefs.getFloat("rTop", rTop);
            rBottom = _prefs.getFloat("rBottom", rBottom);
            reportMode = ReportModeUtil::fromStored(_prefs.getULong("report_mode", (unsigned long)ReportMode::MQTT));
            reedMode = ReedModeUtil::fromStored(_prefs.getULong("reed_mode", 0));
            singleOpenLevel = ReedModeUtil::openLevelFromStored(_prefs.getULong("single_open", 0));
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
    DEBUG_PRINTLN("  rTop (kΩ): " + String(rTop));
    DEBUG_PRINTLN("  rBottom (kΩ): " + String(rBottom));

    DEBUG_PRINTLN("  report_mode: " + String(ReportModeUtil::toText(reportMode)));
    DEBUG_PRINTLN("  reed_mode: " + String(ReedModeUtil::toText(reedMode)));
    DEBUG_PRINTLN("  single_open: " + String(ReedModeUtil::toText(singleOpenLevel)));

    // BLE reporting needs no broker; MQTT reporting still requires a host
    return (reportMode == ReportMode::BLE || mqttHost.length() > 0) && deviceName != "window-sensor";
}

void Config::save() {
    // update fields from portal parameters if they exist
    if (_p_dev != nullptr && _p_dev->getValue() != nullptr) {
        if (!ReportModeUtil::isValidText(_p_report->getValue())) {
            log_e("Invalid reporting mode: use mqtt or ble. Configuration was not saved.");
            return;
        }
        ReedMode requestedMode = reedMode;
        OpenLevel requestedLevel = singleOpenLevel;
        if (!ReedModeUtil::parseMode(_p_reed->getValue(), requestedMode)) {
            log_e("Invalid reed mode: use spdt or single. Configuration was not saved.");
            return;
        }
        if (!ReedModeUtil::parseOpenLevel(_p_open_level->getValue(), requestedLevel)) {
            log_e("Invalid single-reed open level: use HIGH or LOW. Configuration was not saved.");
            return;
        }
        deviceName = _p_dev->getValue();
        mqttHost = _p_host->getValue();
        mqttUser = _p_user->getValue();
        mqttPass = _p_pass->getValue();
        sleepTime = Utils::parseDuration(_p_sleep_time->getValue());
        rTop = String(_p_rTop->getValue()).toFloat();
        rBottom = String(_p_rBottom->getValue()).toFloat();
        reportMode = ReportModeUtil::fromText(_p_report->getValue(), reportMode);
        reedMode = requestedMode;
        singleOpenLevel = requestedLevel;
    }

    _prefs.begin("config", false);
    _prefs.putString("dev", deviceName);
    _prefs.putString("mqtt_host", mqttHost);
    _prefs.putString("mqtt_user", mqttUser);
    _prefs.putString("mqtt_pass", mqttPass);
    _prefs.putULong("sleep_time", sleepTime);
    _prefs.putFloat("rTop", rTop);
    _prefs.putFloat("rBottom", rBottom);
    _prefs.putULong("report_mode", (unsigned long)reportMode);
    _prefs.putULong("reed_mode", (unsigned long)reedMode);
    _prefs.putULong("single_open", (unsigned long)singleOpenLevel);
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
    _p_report_list = new WiFiManagerParameter("<datalist id='report_modes'><option value='mqtt'><option value='ble'></datalist>");
    _p_report = new WiFiManagerParameter("report", "Report via (mqtt or ble)", ReportModeUtil::toText(reportMode), 8, "list='report_modes' autocomplete='off'");
    _p_reed_list = new WiFiManagerParameter("<datalist id='reed_modes'><option value='spdt'><option value='single'></datalist>");
    _p_reed = new WiFiManagerParameter("reed_mode", "Reed wiring (spdt or single on GPIO1)", ReedModeUtil::toText(reedMode), 16, "list='reed_modes' autocomplete='off'");
    _p_open_level_list = new WiFiManagerParameter("<datalist id='open_levels'><option value='HIGH'><option value='LOW'></datalist>");
    _p_open_level = new WiFiManagerParameter("single_open", "Single reed: GPIO1 level when window OPEN (HIGH or LOW)", ReedModeUtil::toText(singleOpenLevel), 16, "list='open_levels' autocomplete='off'");
    _p_host = new WiFiManagerParameter("mqtt", "MQTT Host", mqttHost.c_str(), 64);
    _p_user = new WiFiManagerParameter("user", "MQTT User", mqttUser.c_str(), 32);
    _p_pass = new WiFiManagerParameter("pass", "MQTT Pass", mqttPass.c_str(), 32);
    _p_sleep_time = new WiFiManagerParameter("sleep_time", "Sleep time", Utils::millisToHuman(sleepTime).c_str(), 32);
    // use number inputs so only digits/decimal allowed
    _p_rTop = new WiFiManagerParameter("rTop", "Divider Rₜₒₚ (kΩ)", String(rTop).c_str(), 16, "min=0 step=0.1 type=number");
    _p_rBottom = new WiFiManagerParameter("rBottom", "Divider R₆ₒₜₜₒₘ (kΩ)", String(rBottom).c_str(), 16, "min=0 step=0.1 type=number");

    const char * menu[] = {"wifi","param","sep","info","update","exit"};
    wm.setMenu(menu, 5);
    wm.setDarkMode(true);

    // Add custom head element with navigation and save redirect
    wm.setCustomHeadElement("<style>"
        ".nav-bar { padding: 20px; margin: 20px 0; border: 1px solid #555; background-color: #282828; text-align: center; border-radius: .3rem; }"
        ".nav-bar a { margin: 0 5px; padding: 5px 10px; text-decoration: none; border-radius: 3px; display: inline-block; }"
        ".nav-bar a.active { background: #007bff; color: white; }"
        ".nav-bar a:not(.active) { background: #989fa5; color: white; }"
        "</style>"
        "<script>"
        "document.addEventListener('DOMContentLoaded', function() {"
        "    var navHtml = '<div class=\"nav-bar\">"
        "<a href=\"/\" class=\"' + (window.location.pathname === '/' ? 'active' : '') + '\">Home</a>"
        "<a href=\"/wifi\" class=\"' + (window.location.pathname === '/wifi' ? 'active' : '') + '\">WiFi</a>"
        "<a href=\"/info\" class=\"' + (window.location.pathname === '/info' ? 'active' : '') + '\">Info</a>"
        "<a href=\"/param\" class=\"' + (window.location.pathname === '/param' ? 'active' : '') + '\">Params</a>"
        "<a href=\"/update\" class=\"' + (window.location.pathname === '/update' ? 'active' : '') + '\">Update</a>"
        "</div>';"
        "    var body = document.querySelector('body');"
        "    if (body) {"
        "        body.insertAdjacentHTML('afterbegin', navHtml);"
        "    }"
        "    var saveBtn = document.querySelector('input[type=\"submit\"][value=\"Save\"]');"
        "    if (saveBtn) {"
        "        saveBtn.addEventListener('click', function() {"
        "            setTimeout(function() { window.location.href = '/'; }, 2000);"
        "        });"
        "    }"
        "});"
        "</script>");
    wm.addParameter(_p_group_general);
    wm.addParameter(_p_dev);
    wm.addParameter(_p_report_list);
    wm.addParameter(_p_report);
    wm.addParameter(_p_reed_list);
    wm.addParameter(_p_reed);
    wm.addParameter(_p_open_level_list);
    wm.addParameter(_p_open_level);

    wm.addParameter(_p_group_mqtt);
    wm.addParameter(_p_host);
    wm.addParameter(_p_user);
    wm.addParameter(_p_pass);

    wm.addParameter(_p_adv_begin);
    wm.addParameter(_p_sleep_time);
    wm.addParameter(_p_rTop);
    wm.addParameter(_p_rBottom);
    wm.addParameter(_p_adv_end);

    wm.setConfigPortalTimeout(0);
    wm.setShowStaticFields(true);
    wm.setShowDnsFields(true);
}
