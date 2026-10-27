#include "Config.h"
#include "WindowSensorDefines.h"
#include <cmath>
#include <nvs.h>

Config::Config() : deviceName("window-sensor"),
      mqttHost(""),
      mqttUser(""),
      mqttPass(""),    
      sleepTime(5UL * 60UL * 1000UL),  // 5 min default
      rTop(1000.0),
      rBottom(470.0),
      batteryCorrectionFactor(1.052f),
      blePortalScanEnabled(false),
      reedMode(ReedMode::SINGLE),
    singleOpenLevel(OpenLevel::Low) {
}

bool Config::load() {
    bool configured = false;
    bool legacyWiFiLoaded = false;
    bool portalWiFiLoaded = false;
    portalRequested = false;
    wifiSsid = "";
    wifiPassword = "";
    nvs_handle_t legacyHandle;
    if (nvs_open("nvs.net80211", NVS_READONLY, &legacyHandle) == ESP_OK) {
        uint8_t storedSsid[36] = {};
        char storedPassword[66] = {};
        size_t ssidSize = 0;
        size_t passwordSize = 0;
        const esp_err_t ssidResult = nvs_get_blob(legacyHandle, "sta.ssid", nullptr, &ssidSize);
        esp_err_t passwordResult = nvs_get_str(legacyHandle, "sta.pswd", nullptr, &passwordSize);
        const bool passwordIsString = passwordResult == ESP_OK;
        if (passwordResult != ESP_OK) {
            passwordResult = nvs_get_blob(legacyHandle, "sta.pswd", nullptr, &passwordSize);
        }
        if (ssidResult == ESP_OK && (ssidSize == 32 || ssidSize == sizeof(storedSsid)) &&
            passwordResult == ESP_OK && passwordSize <= sizeof(storedPassword) - 1 &&
            nvs_get_blob(legacyHandle, "sta.ssid", storedSsid, &ssidSize) == ESP_OK &&
            (passwordIsString ? nvs_get_str(legacyHandle, "sta.pswd", storedPassword, &passwordSize)
                              : nvs_get_blob(legacyHandle, "sta.pswd", storedPassword, &passwordSize)) == ESP_OK) {
            if (ssidSize == 32) {
                wifiSsid.concat(reinterpret_cast<const char*>(storedSsid), strnlen(reinterpret_cast<const char*>(storedSsid), 32));
            } else {
                uint32_t ssidLength = 0;
                memcpy(&ssidLength, storedSsid, sizeof(ssidLength));
                if (ssidLength > 0 && ssidLength <= 32) {
                    wifiSsid.concat(reinterpret_cast<const char*>(storedSsid + sizeof(ssidLength)), ssidLength);
                }
            }
            if (!wifiSsid.isEmpty()) {
                wifiPassword = storedPassword;
                legacyWiFiLoaded = true;
            }
        }
        DEBUG_PRINTLN(String("[Config] Legacy Wi-Fi NVS: SSID bytes=") + String(ssidSize) +
                      ", SSID read=" + esp_err_to_name(ssidResult) + ", password read=" + esp_err_to_name(passwordResult) +
                      ", password type=" + (passwordIsString ? "string" : "blob") +
                      ", usable=" + (legacyWiFiLoaded ? "yes" : "no"));
        nvs_close(legacyHandle);
    }
    wifiConfigured = !wifiSsid.isEmpty();
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
            batteryCorrectionFactor = _prefs.getFloat("bat_factor", 1.0f);
            if (!std::isfinite(batteryCorrectionFactor) || batteryCorrectionFactor <= 0.0f) {
                batteryCorrectionFactor = 1.0f;
            }
            reedMode = ReedModeUtil::fromStored(_prefs.getULong("reed_mode", 0));
            blePortalScanEnabled = _prefs.getBool("ble_portal_scan", false);
            singleOpenLevel = ReedModeUtil::openLevelFromStored(_prefs.getULong("single_open", 0));
        }
        const String portalSsid = _prefs.getString("wifi_ssid", "");
        portalWiFiLoaded = !portalSsid.isEmpty() && _prefs.getBool("wifi_set", true);
        if (portalWiFiLoaded) {
            wifiSsid = portalSsid;
            wifiPassword = _prefs.getString("wifi_pass", "");
        }
        wifiConfigured = !wifiSsid.isEmpty();
        staticEnabled = _prefs.getBool("static", false);
        staticIp = _prefs.getString("ip", "");
        gateway = _prefs.getString("gateway", "");
        subnet = _prefs.getString("subnet", "");
        dns = _prefs.getString("dns", "");
        portalPassword = _prefs.getString("portal_pass", "admin");
        portalRequested = _prefs.getBool("portal_req", false);
        configured = _prefs.getBool("configured", wifiConfigured || (!deviceName.isEmpty() && deviceName != "window-sensor"));
        _prefs.end();
    } else {
        // Set fallback device name if NVS is empty
        deviceName = "window-sensor";
        configured = wifiConfigured;
    }

    DEBUG_PRINTLN(String("[Config] Wi-Fi source=") + (portalWiFiLoaded ? "portal" : legacyWiFiLoaded ? "legacy SDK" : "none") +
                  ", enabled=" + (wifiConfigured ? "yes" : "no") + ", SSID=\"" + wifiSsid + "\", authentication=" +
                  (wifiPassword.isEmpty() ? "open/unset" : "password set"));
    return configured;
}

bool Config::save() {
    if (!_prefs.begin("config", false)) return false;
    bool saved = true;
    saved = (_prefs.putString("dev", deviceName) == deviceName.length()) && saved;
    saved = (_prefs.putString("mqtt_host", mqttHost) == mqttHost.length()) && saved;
    saved = (_prefs.putString("mqtt_user", mqttUser) == mqttUser.length()) && saved;
    saved = (_prefs.putString("mqtt_pass", mqttPass) == mqttPass.length()) && saved;
    saved = (_prefs.putULong("sleep_time", sleepTime) == sizeof(uint32_t)) && saved;
    saved = (_prefs.putFloat("rTop", rTop) == sizeof(float)) && saved;
    saved = (_prefs.putFloat("rBottom", rBottom) == sizeof(float)) && saved;
    saved = (_prefs.putFloat("bat_factor", batteryCorrectionFactor) == sizeof(float)) && saved;
    saved = (_prefs.putBool("ble_portal_scan", blePortalScanEnabled) == 1) && saved;
    saved = (_prefs.putULong("reed_mode", static_cast<unsigned long>(reedMode)) == sizeof(uint32_t)) && saved;
    saved = (_prefs.putULong("single_open", static_cast<unsigned long>(singleOpenLevel)) == sizeof(uint32_t)) && saved;
    saved = (_prefs.putString("wifi_ssid", wifiSsid) == wifiSsid.length()) && saved;
    saved = (_prefs.putString("wifi_pass", wifiPassword) == wifiPassword.length()) && saved;
    saved = (_prefs.putBool("wifi_set", wifiConfigured) == 1) && saved;
    saved = (_prefs.putBool("static", staticEnabled) == 1) && saved;
    saved = (_prefs.putString("ip", staticIp) == staticIp.length()) && saved;
    saved = (_prefs.putString("gateway", gateway) == gateway.length()) && saved;
    saved = (_prefs.putString("subnet", subnet) == subnet.length()) && saved;
    saved = (_prefs.putString("dns", dns) == dns.length()) && saved;
    saved = (_prefs.putString("portal_pass", portalPassword) == portalPassword.length()) && saved;
    saved = (_prefs.putBool("portal_req", portalRequested) == 1) && saved;
    if (saved) saved = _prefs.putBool("configured", true) == 1;
    _prefs.end();
    return saved;
}

bool Config::setPortalRequested(bool requested) {
    if (!_prefs.begin("config", false)) return false;
    const bool saved = _prefs.putBool("portal_req", requested) == 1;
    _prefs.end();
    if (saved) portalRequested = requested;
    return saved;
}

