#include "ButtonMonitor.h"
#include "ButtonHold.h"
#include "Hardware.h"
#include "WindowSensorDefines.h"
#include <atomic>

static ButtonHold s_hold;
static std::atomic<bool> s_edge{false};

static void IRAM_ATTR onButtonEdge() {
    s_edge.store(true, std::memory_order_relaxed);
}

void ButtonMonitor::begin() {
    s_hold.reset();
    s_edge.store(false, std::memory_order_relaxed);
    attachInterrupt(digitalPinToInterrupt(Hardware::PIN_BUTTON), onButtonEdge, CHANGE);
}

bool ButtonMonitor::service() {
    bool edge = s_edge.exchange(false, std::memory_order_relaxed);
    bool pressed = digitalRead(Hardware::PIN_BUTTON) == LOW;
    if (edge || pressed || s_hold.pending()) {
        s_hold.update(pressed, millis());
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
