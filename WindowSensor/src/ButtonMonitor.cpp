#include "ButtonMonitor.h"
#include "ButtonHold.h"
#include "Hardware.h"
#include "WindowSensorDefines.h"
#include <atomic>
#include <WiFi.h>
#include <nvs_flash.h>

static ButtonHold s_hold;
static std::atomic<bool> s_edge{false};
static bool s_resetAttempted = false;

static void IRAM_ATTR onButtonEdge() {
    s_edge.store(true, std::memory_order_relaxed);
}

void ButtonMonitor::begin() {
    s_hold.reset();
    s_resetAttempted = false;
    s_edge.store(false, std::memory_order_relaxed);
    attachInterrupt(digitalPinToInterrupt(Hardware::PIN_BUTTON), onButtonEdge, CHANGE);
}

bool ButtonMonitor::service() {
    bool edge = s_edge.exchange(false, std::memory_order_relaxed);
    bool pressed = digitalRead(Hardware::PIN_BUTTON) == LOW;
    if (edge || pressed || s_hold.pending() || s_hold.fired()) {
        s_hold.update(pressed, millis());
    }
    if (s_hold.factoryResetFired() && !s_resetAttempted) {
        s_resetAttempted = true;
        if (WiFi.getMode() != WIFI_OFF && !WiFi.mode(WIFI_OFF)) {
            log_e("Cannot stop Wi-Fi for factory reset. Restart and try again.");
            return s_hold.fired();
        }
        const esp_err_t result = nvs_flash_erase();
        if (result != ESP_OK) {
            log_e("Factory reset failed: %s", esp_err_to_name(result));
        }
        uint32_t releasedAt = millis();
        while (static_cast<uint32_t>(millis() - releasedAt) < ButtonHold::RELEASE_DEBOUNCE_MS) {
            if (digitalRead(Hardware::PIN_BUTTON) == LOW) releasedAt = millis();
            delay(SERVICE_INTERVAL_MS);
        }
        ESP.restart();
    }
    return s_hold.fired();
}

bool ButtonMonitor::holdPending() {
    service();
    return s_hold.pending();
}

bool ButtonMonitor::maintenanceRequested() {
    return s_hold.fired();
}

bool ButtonMonitor::settle() {
    while (holdPending()) {
        delay(SERVICE_INTERVAL_MS);
    }
    return maintenanceRequested();
}

bool ButtonMonitor::delayServicing(uint32_t ms) {
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < ms) {
        if (service()) return true;
        delay(SERVICE_INTERVAL_MS);
    }
    return service();
}
