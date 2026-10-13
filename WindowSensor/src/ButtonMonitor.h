#pragma once

#include <Arduino.h>

// Tracks the GPIO5 maintenance button (active low). The interrupt only sets a flag;
// hold timing and all heavy work happen in the main context via service().
class ButtonMonitor {
public:
    static constexpr uint32_t SERVICE_INTERVAL_MS = 10;

    static void begin();
    // Samples the button and updates hold tracking. Returns true once a 3 s hold has been seen (latched).
    static bool service();
    // true while the button is down and the hold has not been decided yet; do not sleep in this state
    static bool holdPending();
    static bool maintenanceRequested();
    // Blocks (servicing the button) until the button is released or the hold completes.
    // Returns maintenanceRequested().
    static bool settle();
    // Delay that keeps servicing the button; returns early when maintenance has been requested.
    static bool delayServicing(uint32_t ms);
};
