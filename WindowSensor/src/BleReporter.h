#pragma once

#include <Arduino.h>
#include "Config.h"
#include "Hardware.h"

// Reports window state and battery as a short BTHome v2 advertisement burst.
// BLE is shut down before Wi-Fi starts.
class BleReporter {
public:
    static constexpr uint32_t BURST_MS = 1000;
    static constexpr uint16_t ADV_INTERVAL_MIN = 160; // 100 ms in 0.625 ms units
    static constexpr uint16_t ADV_INTERVAL_MAX = 160; // 320; // 200 ms

    explicit BleReporter(Config& cfg);

    // Scans passively and returns true when the portal request targets this device's BLE MAC.
    bool scanForPortalRequest(uint32_t durationMs);

    // Returns true when the burst was sent completely. Returns false on a BLE error or when the
    // maintenance button hold interrupted the burst (check ButtonMonitor::maintenanceRequested()).
    // The BLE stack is always shut down again before returning.
    bool report(const Hardware::WindowState& state, float batteryVoltage);

    // Safe to call at any time (also when nothing is running).
    void shutdown();

private:
    Config& _config;
    bool _stackInitialized = false;
    bool _advertising = false;

    bool _start();
    bool _advertise(const uint8_t* data, size_t len);
};


