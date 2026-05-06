#pragma once

#include <Arduino.h>

class Hardware {
public:

    enum class WindowState : uint8_t {
        OPEN = 2,    // PIN_NC
        CLOSED = 1   // PIN_NO
    };

    // helper to convert window state to boolean if needed
    static constexpr bool toBool(WindowState state) {
        return state == WindowState::OPEN;
    }

    static constexpr int PIN_NO = 1;
    static constexpr int PIN_NC = 2;
    static constexpr int PIN_BAT_ADC = 4;
    static constexpr int PIN_BAT_EN = 3;
    static constexpr int PIN_BUTTON = 5;
    static constexpr int BATTERY_SAMPLES = 5;

    static void initPins();
    static WindowState readWindowState();
    static float readBattery(float rTop, float rBottom);
    static bool bootButtonPressed();
};
