#include "WiFiConnector.h"
#include "FirmwareManager.h"
#include "Hardware.h"
#include "ButtonMonitor.h"
#include "version.h"
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <cmath>
#include <new>

extern const char portalHtml[] asm("_binary_ui_portal_html_start");

namespace {
constexpr size_t LOG_CAPACITY = 64;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

const char* wifiStatusName(wl_status_t status) {
    switch (status) {
        case WL_IDLE_STATUS: return "idle/connecting";
        case WL_NO_SSID_AVAIL: return "network not found";
        case WL_SCAN_COMPLETED: return "scan completed";
        case WL_CONNECTED: return "connected";
        case WL_CONNECT_FAILED: return "connection failed";
        case WL_CONNECTION_LOST: return "connection lost";
        case WL_DISCONNECTED: return "disconnected";
        default: return "unavailable";
    }
}

const char* wifiReasonName(uint16_t reason) {
    switch (reason) {
        case WIFI_REASON_AUTH_EXPIRE: return "authentication expired";
        case WIFI_REASON_ASSOC_LEAVE: return "station disconnected";
        case WIFI_REASON_BEACON_TIMEOUT: return "access point beacon timeout";
        case WIFI_REASON_NO_AP_FOUND: return "network not found (check SSID and 2.4 GHz coverage)";
        case WIFI_REASON_AUTH_FAIL: return "authentication failed (check password/security)";
        case WIFI_REASON_ASSOC_FAIL: return "association failed";
        case WIFI_REASON_HANDSHAKE_TIMEOUT: return "handshake timeout (check password/signal)";
        default: return "see ESP-IDF Wi-Fi reason code";
    }
}

void sendJson(WebServer& server, const JsonDocument& document, int code = 200) {
    String body;
    serializeJson(document, body);
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(code, "application/json", body);
}

bool validName(const String& name) {
    if (name.isEmpty() || name.length() > 32 || name[0] == '-' || name[name.length() - 1] == '-') return false;
    for (const char character : name) {
        if (!(character >= 'a' && character <= 'z') && !(character >= 'A' && character <= 'Z') &&
            !(character >= '0' && character <= '9') && character != '-') return false;
    }
    return true;
}

bool textField(JsonVariantConst field, size_t maxLength) {
    if (!field.is<const char*>()) return false;
    const String text = field.as<String>();
    if (text.length() > maxLength) return false;
    for (const char character : text) {
        if (static_cast<unsigned char>(character) < 32) return false;
    }
    return true;
}

bool numberField(JsonVariantConst field, double minimum, double maximum) {
    if (!field.is<double>()) return false;
    const double value = field.as<double>();
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
}

WiFiConnector::WiFiConnector(Config& cfg) : _config(cfg) {}

void WiFiConnector::message(int code, const String& text) {
    JsonDocument document;
    document["message"] = text;
    sendJson(*_server, document, code);
}

bool WiFiConnector::authorized(bool mutation) {
    String host = _server->hostHeader();
    host.toLowerCase();
    if (host.endsWith(":80")) host.remove(host.length() - 3);
    String hostname = _hostname;
    hostname.toLowerCase();
    if (host != WiFi.localIP().toString() && host != WiFi.softAPIP().toString() &&
        host != hostname && host != hostname + ".local") {
        message(403, "Unrecognized portal address");
        return false;
    }
    if (!_server->authenticate("admin", _password.c_str())) {
        _server->requestAuthentication(BASIC_AUTH, "Window Sensor");
        return false;
    }
    if (mutation && _server->header("X-Portal-Token") != _token) {
        message(403, "Session expired. Reload the page.");
        return false;
    }
    if (mutation && _factoryResetRequested) {
        message(409, "Factory reset is already in progress.");
        return false;
    }
    return true;
}

void WiFiConnector::record(const String& text) {
    DEBUG_PRINTLN("[Portal] " + text);
    if (!_logEnabled || !_logs) return;
    LogLine& line = _logs[_logId % LOG_CAPACITY];
    line.id = ++_logId;
    snprintf(line.text, sizeof(line.text), "[%lus] %s", (millis() - _started) / 1000UL, text.c_str());
    if (_logCount < LOG_CAPACITY) ++_logCount;
}

bool WiFiConnector::startStation() {
    if (!_config.wifiConfigured || _config.wifiSsid.isEmpty()) return false;
    if (!WiFi.mode(_ap ? WIFI_AP_STA : WIFI_STA)) {
        record("Wi-Fi station mode failed to start");
        return false;
    }
    WiFi.setSleep(true);
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(false, false);
    if (_config.staticEnabled) {
        IPAddress address, gateway, subnet, dns;
        if (!address.fromString(_config.staticIp) || !gateway.fromString(_config.gateway) ||
            !subnet.fromString(_config.subnet) || !dns.fromString(_config.dns)) {
            record("Wi-Fi static IP settings are invalid");
            return false;
        }
        if (!WiFi.config(address, gateway, subnet, dns)) {
            record("Wi-Fi static IP configuration failed");
            return false;
        }
        record("Wi-Fi static IP=" + address.toString() + ", gateway=" + gateway.toString() + ", subnet=" + subnet.toString());
    } else if (!WiFi.config(IPAddress(), IPAddress(), IPAddress())) {
        record("Wi-Fi DHCP configuration failed");
        return false;
    }
    WiFi.setMinSecurity(_config.wifiPassword.isEmpty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK);
    record("Wi-Fi connecting to \"" + _config.wifiSsid + "\" (up to 20 seconds; " +
           String(_config.wifiPassword.isEmpty() ? "open network" : "password set") + ")");
    _connectionStarted = millis();
    const wl_status_t initial = WiFi.begin(_config.wifiSsid.c_str(), _config.wifiPassword.c_str());
    record("Wi-Fi begin returned " + String(static_cast<int>(initial)) + " (" + wifiStatusName(initial) + ")");
    _connecting = true;
    return true;
}

void WiFiConnector::serviceWiFi() {
    if (_disconnectPending.exchange(false, std::memory_order_acquire)) {
        const uint16_t reason = _disconnectReason.load(std::memory_order_relaxed);
        record("Wi-Fi disconnect reason=" + String(reason) + " (" + wifiReasonName(reason) + ")");
    }
    const wl_status_t current = WiFi.status();
    if (_connecting && current == WL_CONNECTED) _connecting = false;
    if (_connecting && millis() - _connectionStarted >= WIFI_CONNECT_TIMEOUT_MS) {
        _connecting = false;
        record("Wi-Fi connection timed out; provisioning AP will remain available");
    }
    if (current == _lastWiFiStatus) return;
    _lastWiFiStatus = current;
    record("Wi-Fi status=" + String(static_cast<int>(current)) + " (" + wifiStatusName(current) + ")");
    if (current == WL_CONNECTED) {
        record("Wi-Fi connected to \"" + WiFi.SSID() + "\": IP=" + WiFi.localIP().toString() +
               ", gateway=" + WiFi.gatewayIP().toString() + ", DNS=" + WiFi.dnsIP().toString() +
               ", RSSI=" + String(WiFi.RSSI()) + " dBm");
    }
}

void WiFiConnector::status() {
    const auto window = Hardware::readWindowState(_config.reedMode, _config.singleOpenLevel);
    const auto battery = Hardware::readBattery(_config.rTop, _config.rBottom, _config.batteryCorrectionFactor);
    JsonDocument document;
    document["deviceName"] = _config.deviceName;
    document["version"] = VERSION;
    document["build"] = __DATE__ " " __TIME__;
    document["windowOpen"] = window.isOpen();
    document["windowValid"] = window.valid();
    document["battery"] = battery.batteryVoltage;
    document["ssid"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String();
    document["rssi"] = WiFi.RSSI();
    document["ip"] = (_ap ? WiFi.softAPIP() : WiFi.localIP()).toString();
    document["mac"] = WiFi.macAddress();
    document["mqttHost"] = _config.mqttHost;
    document["heap"] = ESP.getFreeHeap();
    const uint32_t elapsed = (millis() - _started) / 1000UL;
    document["remaining"] = _timeoutSec ? static_cast<int32_t>(_timeoutSec > elapsed ? _timeoutSec - elapsed : 0) : -1;
    document["sleepSeconds"] = _config.sleepTime / 1000UL;
    document["reedMode"] = ReedModeUtil::toText(_config.reedMode);
    document["singleOpenLevel"] = ReedModeUtil::toText(_config.singleOpenLevel);
    document["ap"] = _ap;
    document["logEnabled"] = _logEnabled;
    document["wifiStatus"] = wifiStatusName(WiFi.status());
    document["wifiDisconnectReason"] = _disconnectReason.load(std::memory_order_relaxed);
    sendJson(*_server, document);
}

void WiFiConnector::settings() {
    JsonDocument document;
    document["deviceName"] = _config.deviceName;
    document["mqttHost"] = _config.mqttHost;
    document["mqttUser"] = _config.mqttUser;
    document["sleepSeconds"] = _config.sleepTime / 1000UL;
    document["rTop"] = _config.rTop;
    document["rBottom"] = _config.rBottom;
    document["batteryCorrectionFactor"] = _config.batteryCorrectionFactor;
    document["blePortalScanEnabled"] = _config.blePortalScanEnabled;
    document["reedMode"] = ReedModeUtil::toText(_config.reedMode);
    document["singleOpenLevel"] = ReedModeUtil::toText(_config.singleOpenLevel);
    document["ssid"] = _config.wifiSsid;
    document["wifiOpen"] = _config.wifiConfigured && _config.wifiPassword.isEmpty();
    document["staticEnabled"] = _config.staticEnabled;
    document["staticIp"] = _config.staticIp;
    document["gateway"] = _config.gateway;
    document["subnet"] = _config.subnet;
    document["dns"] = _config.dns;
    sendJson(*_server, document);
}

void WiFiConnector::saveSettings() {
    if (_server->arg("plain").length() > 2048) {
        message(413, "Settings request is too large");
        return;
    }
    JsonDocument document;
    if (deserializeJson(document, _server->arg("plain")) || !document.is<JsonObject>()) {
        message(400, "Invalid settings JSON");
        return;
    }
    Config pending;
    pending.load();
    pending.wifiSsid = _config.wifiSsid;
    pending.wifiPassword = _config.wifiPassword;
    pending.wifiConfigured = _config.wifiConfigured;
    const String section = document["section"] | "";
    if (section == "device") {
        ReedMode mode;
        OpenLevel level;
        if (!textField(document["deviceName"], 32) || !validName(document["deviceName"].as<String>()) ||
            !document["sleepSeconds"].is<uint32_t>() || document["sleepSeconds"].as<uint32_t>() > 4294967UL ||
            !numberField(document["rTop"], 0, 1000000) || !numberField(document["rBottom"], 0.000001, 1000000) ||
            !numberField(document["batteryCorrectionFactor"], 0.000001, 100) ||
            !document["blePortalScanEnabled"].is<bool>() ||
            !ReedModeUtil::parseMode(document["reedMode"], mode) ||
            !ReedModeUtil::parseOpenLevel(document["singleOpenLevel"], level) ||
            !textField(document["portalPassword"], 63)) {
            message(400, "Check the device name, interval, reed wiring, and calibration values.");
            return;
        }
        const String password = document["portalPassword"].as<String>();
        if (!password.isEmpty() && password.length() < 8) {
            message(400, "The maintenance password must have at least 8 characters.");
            return;
        }
        pending.deviceName = document["deviceName"].as<String>();
        pending.sleepTime = document["sleepSeconds"].as<uint32_t>() * 1000UL;
        pending.rTop = document["rTop"].as<float>();
        pending.rBottom = document["rBottom"].as<float>();
        pending.batteryCorrectionFactor = document["batteryCorrectionFactor"].as<float>();
        pending.blePortalScanEnabled = document["blePortalScanEnabled"].as<bool>();
        pending.reedMode = mode;
        pending.singleOpenLevel = level;
        if (!password.isEmpty()) pending.portalPassword = password;
    } else if (section == "network") {
        if (!textField(document["ssid"], 32) || !textField(document["mqttHost"], 64) ||
            !textField(document["mqttUser"], 32) || !textField(document["mqttPass"], 64) ||
            !document["wifiOpen"].is<bool>() || !document["clearMqttPass"].is<bool>() ||
            !document["staticEnabled"].is<bool>() ||
            (!document["wifiPassword"].isNull() && !textField(document["wifiPassword"], 63))) {
            message(400, "Invalid network settings");
            return;
        }
        const String ssid = document["ssid"].as<String>();
        const bool changedNetwork = ssid != _config.wifiSsid;
        String password = document["wifiPassword"] | "";
        if (document["wifiOpen"].as<bool>()) password = "";
        else if (password.isEmpty() && !changedNetwork) password = _config.wifiPassword;
        else if (password.length() < 8) {
            message(400, "Enter a password of at least 8 characters, or select Open network.");
            return;
        }
        if (ssid.isEmpty()) {
            message(400, "Enter a Wi-Fi network name.");
            return;
        }
        pending.wifiSsid = ssid;
        pending.wifiPassword = password;
        pending.wifiConfigured = true;
        pending.mqttHost = document["mqttHost"].as<String>();
        pending.mqttUser = document["mqttUser"].as<String>();
        if (document["clearMqttPass"].as<bool>()) pending.mqttPass = "";
        else if (!document["mqttPass"].as<String>().isEmpty()) pending.mqttPass = document["mqttPass"].as<String>();
        pending.staticEnabled = document["staticEnabled"].as<bool>();
        if (pending.staticEnabled) {
            for (const char* key : {"staticIp", "gateway", "subnet", "dns"}) {
                IPAddress address;
                if (!textField(document[key], 15) || !address.fromString(document[key].as<String>()) ||
                    address == IPAddress(0, 0, 0, 0) || address == IPAddress(255, 255, 255, 255)) {
                    message(400, "Enter valid IPv4 addresses for the static network.");
                    return;
                }
            }
            pending.staticIp = document["staticIp"].as<String>();
            pending.gateway = document["gateway"].as<String>();
            pending.subnet = document["subnet"].as<String>();
            pending.dns = document["dns"].as<String>();
        }
    } else {
        message(400, "Unknown settings section");
        return;
    }
    if (!pending.save()) {
        message(503, "Could not persist settings. Check the device before restarting.");
        return;
    }
    _config.load();
    record(section + " settings saved");
    if (section == "network") {
        if (_config.wifiSsid != pending.wifiSsid || _config.wifiPassword != pending.wifiPassword || !_config.wifiConfigured) {
            record("Wi-Fi settings read-back verification failed");
            message(503, "Saved Wi-Fi settings could not be verified. Please retry.");
            return;
        }
        record("Wi-Fi settings verified in flash for SSID=\"" + _config.wifiSsid + "\"");
        if (_ap) {
            _connectRequested = true;
            _connectAt = millis() + 750UL;
            message(200, "Network settings saved. Connecting now; the setup AP remains available.");
            return;
        }
    }
    message(200, "Settings saved. Restart to apply all changes.");
}

void WiFiConnector::routes() {
    const char* headers[] = {"X-Portal-Token"};
    _server->collectHeaders(headers, 1);
    _server->on("/", HTTP_GET, [this]() {
        if (!authorized()) return;
        _server->sendHeader("Cache-Control", "no-cache");
        _server->sendHeader("X-Content-Type-Options", "nosniff");
        _server->sendHeader("X-Frame-Options", "DENY");
        _server->sendHeader("Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' blob: data:; connect-src 'self'; frame-ancestors 'none'");
        _server->send_P(200, "text/html; charset=utf-8", portalHtml);
    });
    _server->on("/api/session", HTTP_GET, [this]() {
        if (!authorized()) return;
        JsonDocument document;
        document["token"] = _token;
        sendJson(*_server, document);
    });
    _server->on("/api/status", HTTP_GET, [this]() { if (authorized()) status(); });
    _server->on("/api/config", HTTP_GET, [this]() { if (authorized()) settings(); });
    _server->on("/api/config", HTTP_POST, [this]() { if (authorized(true)) saveSettings(); });
    _server->on("/api/scan", HTTP_GET, [this]() {
        if (!authorized()) return;
        const int count = WiFi.scanNetworks(false, false, false, 120);
        JsonDocument document;
        JsonArray networks = document["networks"].to<JsonArray>();
        for (int index = 0; index < count && index < 24; ++index) {
            JsonObject network = networks.add<JsonObject>();
            network["ssid"] = WiFi.SSID(index);
            network["rssi"] = WiFi.RSSI(index);
            network["open"] = WiFi.encryptionType(index) == WIFI_AUTH_OPEN;
        }
        WiFi.scanDelete();
        record("Wi-Fi scan complete");
        if (count < 0) message(503, "Wi-Fi scan failed. Try again.");
        else sendJson(*_server, document);
    });
    _server->on("/api/battery-debug", HTTP_POST, [this]() {
        if (!authorized(true)) return;
        const bool sent = _firmwareManager && _firmwareManager->publishBatteryDebug();
        record(sent ? "Battery diagnostic published via MQTT" : "Battery diagnostic publish failed");
        message(sent ? 200 : 503, sent ? "Battery debug sent via MQTT" : "MQTT publish failed. Check the connection and settings.");
    });
    _server->on("/api/restart", HTTP_POST, [this]() {
        if (!authorized(true)) return;
        record("Restart requested");
        message(200, "Restarting. Maintenance is ending.");
        _restartRequested = true;
        _restartAt = millis() + 750UL;
    });
    _server->on("/api/restart-portal", HTTP_POST, [this]() {
        if (!authorized(true)) return;
        if (_uploading || _restartRequested) {
            message(409, "An update or restart is already in progress.");
            return;
        }
        if (!_config.setPortalRequested(true)) {
            message(503, "Could not save the portal request. The device was not restarted.");
            return;
        }
        record("Restart into portal requested");
        message(200, "Restarting into the portal using saved settings.");
        _restartRequested = true;
        _restartAt = millis() + 750UL;
    });
    _server->on("/api/factory-reset", HTTP_POST, [this]() {
        if (!authorized(true)) return;
        if (_uploading || _restartRequested || Update.isRunning()) {
            message(409, "An update or restart is already in progress.");
            return;
        }
        JsonDocument document;
        if (_server->arg("plain").length() > 64 || deserializeJson(document, _server->arg("plain")) ||
            document["confirmation"].as<String>() != "RESET") {
            message(400, "Confirm the factory reset by entering RESET.");
            return;
        }
        record("Factory reset requested: all NVS settings and Wi-Fi credentials will be erased");
        message(200, "Factory reset scheduled. Reconnect to the setup AP after the device restarts.");
        _factoryResetRequested = true;
        _restartRequested = true;
        _restartAt = millis() + 1000UL;
    });
    _server->on("/api/logs", HTTP_POST, [this]() {
        if (!authorized(true)) return;
        JsonDocument document;
        if (_server->arg("plain").length() > 64 || deserializeJson(document, _server->arg("plain")) ||
            !document["enabled"].is<bool>()) {
            message(400, "Invalid capture setting");
            return;
        }
        const bool enabled = document["enabled"];
        if (enabled && !_logs) _logs.reset(new (std::nothrow) LogLine[LOG_CAPACITY]);
        if (enabled && !_logs) {
            message(503, "Not enough memory for log capture");
            return;
        }
        _logEnabled = enabled;
        record("Portal log capture enabled");
            if (enabled) {
                record("Wi-Fi: " + String(wifiStatusName(WiFi.status())) + "; configured SSID=\"" + _config.wifiSsid + "\"");
                const uint16_t reason = _disconnectReason.load(std::memory_order_relaxed);
                if (reason) record("Last Wi-Fi disconnect reason=" + String(reason) + " (" + wifiReasonName(reason) + ")");
            }
        if (enabled) record("Battery: " + String(Hardware::readBattery(_config.rTop, _config.rBottom, _config.batteryCorrectionFactor).batteryVoltage, 2) + " V");
        message(200, enabled ? "Capture enabled" : "Capture paused");
    });
    _server->on("/api/logs", HTTP_GET, [this]() {
        if (!authorized()) return;
        const uint32_t after = strtoul(_server->arg("after").c_str(), nullptr, 10);
        JsonDocument document;
        JsonArray lines = document["lines"].to<JsonArray>();
        for (size_t offset = 0; _logs && offset < _logCount; ++offset) {
            const LogLine& line = _logs[(_logId - _logCount + offset) % LOG_CAPACITY];
            if (line.id <= after) continue;
            JsonObject entry = lines.add<JsonObject>();
            entry["id"] = line.id;
            entry["text"] = line.text;
        }
        sendJson(*_server, document);
    });
    _server->on("/api/update", HTTP_POST, [this]() {
        if (!authorized(true)) return;
        if (!_uploadAuthorized || !_uploadError.isEmpty()) {
            message(400, _uploadError.isEmpty() ? "No firmware was uploaded" : _uploadError);
            return;
        }
        message(200, "Firmware installed. Restarting device.");
        _restartRequested = true;
        _restartAt = millis() + 750UL;
    }, [this]() {
        HTTPUpload& upload = _server->upload();
        if (upload.status == UPLOAD_FILE_START) {
            _uploadAuthorized = authorized(true);
            _uploadError = "";
            _uploading = _uploadAuthorized;
            if (!_uploadAuthorized) return;
            if (!upload.filename.endsWith(".bin")) _uploadError = "Select a firmware .bin file";
            else if (!Update.begin(UPDATE_SIZE_UNKNOWN)) _uploadError = Update.errorString();
            record("Firmware upload started");
        } else if (_uploadAuthorized && upload.status == UPLOAD_FILE_WRITE && _uploadError.isEmpty()) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) _uploadError = Update.errorString();
        } else if (_uploadAuthorized && upload.status == UPLOAD_FILE_END) {
            if (_uploadError.isEmpty() && !Update.end(true)) _uploadError = Update.errorString();
            if (!_uploadError.isEmpty()) Update.abort();
            _uploading = false;
            record(_uploadError.isEmpty() ? "Firmware upload verified" : "Firmware upload failed");
        } else if (_uploadAuthorized && upload.status == UPLOAD_FILE_ABORTED) {
            Update.abort();
            _uploadError = "Upload interrupted";
            _uploading = false;
            record(_uploadError);
        }
    });
    _server->onNotFound([this]() {
        if (_server->uri().startsWith("/api/")) {
            if (authorized()) message(404, "Unknown endpoint");
            return;
        }
        _server->sendHeader("Location", "http://" + (_ap ? WiFi.softAPIP() : WiFi.localIP()).toString() + "/");
        _server->send(302, "text/plain", "");
    });
}

void WiFiConnector::connect(bool forceConfig, uint32_t portalTimeoutSec, FirmwareManager* firmwareManager) {
    _firmwareManager = firmwareManager;
    _hostname = _config.deviceName;
    _password = _config.portalPassword;
    _timeoutSec = portalTimeoutSec;
    _restartRequested = false;
    _factoryResetRequested = false;
    _connectRequested = false;
    _connecting = false;
    _ap = false;
    _started = millis();
    _lastWiFiStatus = WL_NO_SHIELD;
    _disconnectReason.store(0, std::memory_order_relaxed);
    _disconnectPending.store(false, std::memory_order_relaxed);
    const wifi_event_id_t eventId = WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            _disconnectReason.store(info.wifi_sta_disconnected.reason, std::memory_order_relaxed);
            _disconnectPending.store(true, std::memory_order_release);
        }
    });
    WiFi.setHostname(_hostname.c_str());
    const bool stationConfigured = _config.wifiConfigured && !_config.wifiSsid.isEmpty();
    bool connected = false;
    record("Wi-Fi configuration: enabled=" + String(_config.wifiConfigured ? "yes" : "no") +
           ", SSID=\"" + _config.wifiSsid + "\", authentication=" +
           String(_config.wifiPassword.isEmpty() ? "open" : "password set") +
           ", addressing=" + String(_config.staticEnabled ? "static" : "DHCP"));
    if (stationConfigured) {
        if (startStation()) {
            while (WiFi.status() != WL_CONNECTED && millis() - _connectionStarted < WIFI_CONNECT_TIMEOUT_MS) {
                serviceWiFi();
                ButtonMonitor::service();
                delay(ButtonMonitor::SERVICE_INTERVAL_MS);
            }
        }
        serviceWiFi();
        connected = WiFi.status() == WL_CONNECTED;
        if (!connected) record("Wi-Fi connection timed out; starting provisioning AP");
    } else {
        record("No configured SSID; skipping station connection and starting provisioning AP");
    }
    if (connected && !forceConfig) {
        WiFi.removeEvent(eventId);
        return;
    }
    _ap = !connected;
    if (_ap) {
        WiFi.mode(stationConfigured ? WIFI_AP_STA : WIFI_AP);
        String suffix = WiFi.macAddress();
        suffix.replace(":", "");
        const String apName = "Window-Sensor-" + suffix.substring(6);
        if (!WiFi.softAP(apName.c_str())) {
            record("Provisioning AP failed to start; restarting");
            WiFi.removeEvent(eventId);
            delay(1000);
            ESP.restart();
            return;
        }
        _dnsServer.reset(new DNSServer());
        _dnsServer->start(53, "*", WiFi.softAPIP());
        record("Provisioning AP \"" + apName + "\" ready at " + WiFi.softAPIP().toString());
    }
    if (MDNS.begin(_hostname.c_str())) MDNS.addService("http", "tcp", 80);
    char token[33];
    snprintf(token, sizeof(token), "%08lx%08lx%08lx%08lx", static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    _token = token;
    _server.reset(new WebServer(80));
    _started = millis();
    routes();
    _server->begin();
    if (_firmwareManager) _firmwareManager->maintenancePortalStarted();
    while (true) {
        if (_dnsServer) _dnsServer->processNextRequest();
        _server->handleClient();
        ButtonMonitor::service();
        if (_connectRequested && !_uploading && !_restartRequested && static_cast<int32_t>(millis() - _connectAt) >= 0) {
            _connectRequested = false;
            startStation();
        }
        serviceWiFi();
        if (_firmwareManager && !_uploading && !_restartRequested) _firmwareManager->service();
        if (_restartRequested && static_cast<int32_t>(millis() - _restartAt) >= 0) break;
        if (!_uploading && !_restartRequested && _timeoutSec && (millis() - _started) / 1000UL >= _timeoutSec) break;
        delay(2);
    }
    _server->stop();
    if (_dnsServer) _dnsServer->stop();
    MDNS.end();
    WiFi.removeEvent(eventId);
    _logs.reset();
    _logEnabled = false;
    if (_factoryResetRequested) {
        if (WiFi.mode(WIFI_OFF)) {
            const esp_err_t result = nvs_flash_erase();
            if (result != ESP_OK) log_e("Factory reset failed: %s", esp_err_to_name(result));
            else record("Factory reset complete; restarting into provisioning");
        } else {
            log_e("Factory reset cancelled: Wi-Fi could not be stopped safely.");
        }
    }
    delay(100);
    ESP.restart();
}
