#pragma once

#include <Arduino.h>
#include "WindowSensorDefines.h"
class Hardware {
public:

    struct WindowState {
        bool no;
        bool nc;

        int wakePin() const {
            if (no == LOW) {
                return PIN_NC;
            } else if (nc == LOW) {
                return PIN_NO;
            } else {
                // Both HIGH - should not happen with SPDT, default to CLOSED or handle error
                DEBUG_PRINTLN("Warning: Both NO and NC are HIGH - possible wiring issue");
                return PIN_NC; // default to closed
            }
        }

        bool isOpen() const {
            // read the NO and NC pins and determine window state
            // Assuming SPDT switch: when closed, NO is LOW, NC is HIGH; when open, NO is HIGH, NC is LOW
            if (no == LOW) {
                return true;
            } else if (nc == LOW) {
                return false;
            } else {
                // Both HIGH - should not happen with SPDT, default to CLOSED or handle error
                DEBUG_PRINTLN("Warning: Both NO and NC are HIGH - possible wiring issue");
                return false;
            }            
        }
    };

    static constexpr int PIN_NO = 2;
    static constexpr int PIN_NC = 1;
    static constexpr int PIN_BAT_ADC = 4;
    static constexpr int PIN_BUTTON = 5;
    static constexpr int BATTERY_SAMPLES = 3;

    static void initPins();
    static WindowState readWindowState();
    static float readBattery(float rTop, float rBottom);
    static bool bootButtonPressed();
};
