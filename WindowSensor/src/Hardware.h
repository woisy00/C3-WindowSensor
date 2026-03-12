#pragma once

#include <Arduino.h>

class Hardware {
public:
    static constexpr int PIN_NO = 1;
    static constexpr int PIN_NC = 2;
    static constexpr int PIN_BAT_ADC = 4;
    static constexpr int PIN_BUTTON = 5;
    static constexpr int BATTERY_SAMPLES = 5;

    static void initPins();
    static bool readWindowState();
    static float readBattery(float rTop, float rBottom);
    static bool bootButtonPressed();
};
