#pragma once

#include <Arduino.h>
#include "WindowSensorDefines.h"
class Hardware {
public:

    struct WindowState {
        // low -> false, high -> true
        int no; // low -> window closed, high -> window open
        int nc; // low -> window open, high -> window closed

        int wakePin() const {
            if (no) {
                return PIN_NO;
            } else if (nc) {
                return PIN_NC;
            } else {
                // Both HIGH - should not happen with SPDT, default to CLOSED or handle error
                DEBUG_PRINTLN("Warning: Both NO and NC are HIGH - possible wiring issue");
                return -1; // default to closed
            }
        }

        bool isOpen() const {
            // read the NO and NC pins and determine window state
            // Assuming SPDT switch: when closed, NO is LOW, NC is HIGH; when open, NO is HIGH, NC is LOW
            if (no) {
                return true;
            } else if (nc) {
                return false;
            } else {
                // Both HIGH - should not happen with SPDT, default to CLOSED or handle error
                DEBUG_PRINTLN("Warning: Both NO and NC are HIGH - possible wiring issue");
                return true;
            }            
        }

        bool matches(const WindowState& other) const {
            return no == other.no && nc == other.nc;
        }
    };

    static constexpr int PIN_NO = 2;
    static constexpr int PIN_NC = 1;
    static constexpr int PIN_BAT_ADC = 4;
    static constexpr int PIN_BUTTON = 5;
    static constexpr int BATTERY_SAMPLES = 3;
    static constexpr int DEBOUNCE_SAMPLES = 5;  // number of consistent reads needed
    static constexpr int DEBOUNCE_DELAY_MS = 10; // delay between debounce samples

    static void initPins();
    static WindowState readWindowState();
    static float readBattery(float rTop, float rBottom);
    static bool bootButtonPressed();

    static int readPin(int pin);

private:
    static int readDebouncedPin(int pin);
};
