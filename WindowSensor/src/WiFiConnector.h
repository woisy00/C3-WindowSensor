#pragma once

#include <WebServer.h>
#include <DNSServer.h>
#include <memory>
#include <atomic>
#include "Config.h"

class FirmwareManager;

class WiFiConnector {
public:
    explicit WiFiConnector(Config& cfg);
    // forceConfig: run the (blocking) portal even when Wi-Fi is connected.
    // portalTimeoutSec: 0 = no timeout (first provisioning), otherwise the portal is left after that long.
    void connect(bool forceConfig, uint32_t portalTimeoutSec = 0, FirmwareManager* firmwareManager = nullptr);

private:
    Config& _config;
    std::unique_ptr<WebServer> _server;
    std::unique_ptr<DNSServer> _dnsServer;
    FirmwareManager* _firmwareManager = nullptr;
    String _token;
    String _hostname;
    String _password;
    bool _ap = false;
    bool _restartRequested = false;
    bool _factoryResetRequested = false;
    bool _uploading = false;
    bool _uploadAuthorized = false;
    String _uploadError;
    uint32_t _started = 0;
    uint32_t _timeoutSec = 0;
    uint32_t _restartAt = 0;
    struct LogLine {
        uint32_t id;
        char text[160];
    };
    std::unique_ptr<LogLine[]> _logs;
    size_t _logCount = 0;
    uint32_t _logId = 0;
    bool _logEnabled = false;
    std::atomic<uint16_t> _disconnectReason{0};
    std::atomic<bool> _disconnectPending{false};
    wl_status_t _lastWiFiStatus = WL_NO_SHIELD;
    bool _connectRequested = false;
    bool _connecting = false;
    uint32_t _connectAt = 0;
    uint32_t _connectionStarted = 0;

    bool startStation();
    void serviceWiFi();
    bool authorized(bool mutation = false);
    void routes();
    void status();
    void settings();
    void saveSettings();
    void message(int code, const String& text);
    void record(const String& text);
};
