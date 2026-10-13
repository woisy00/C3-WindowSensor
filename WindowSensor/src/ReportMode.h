#pragma once

#include <string.h>
#include <strings.h>

enum class ReportMode : unsigned char {
    MQTT = 0, // default, also for configs saved by older firmware
    BLE = 1
};

namespace ReportModeUtil {
    inline ReportMode fromStored(unsigned long v) {
        return v == 1 ? ReportMode::BLE : ReportMode::MQTT;
    }

    inline ReportMode fromText(const char* s, ReportMode fallback) {
        if (!s) return fallback;
        while (*s == ' ') s++;
        if (strcasecmp(s, "ble") == 0) return ReportMode::BLE;
        if (strcasecmp(s, "mqtt") == 0) return ReportMode::MQTT;
        return fallback;
    }

    inline bool isValidText(const char* s) {
        return s && (strcasecmp(s, "ble") == 0 || strcasecmp(s, "mqtt") == 0);
    }

    inline const char* toText(ReportMode m) {
        return m == ReportMode::BLE ? "ble" : "mqtt";
    }
}
