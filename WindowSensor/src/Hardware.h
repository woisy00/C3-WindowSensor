#pragma once

#include <Arduino.h>
#include "WindowSensorDefines.h"
#include "ReedMode.h"
class Hardware {
public:

    using WindowState = ReedLogic::State;

    static constexpr int PIN_NO = 2;
    static constexpr int PIN_NC = 1;
    static constexpr int PIN_BAT_ADC = 4;
    static constexpr int PIN_BUTTON = 5;
    static constexpr int BATTERY_SAMPLES = 3;
    static constexpr int DEBOUNCE_SAMPLES = 5;  // number of consistent reads needed
    static constexpr int DEBOUNCE_DELAY_MS = 10; // delay between debounce samples

    static void initPins();
    static WindowState readWindowState(ReedMode mode = ReedMode::SPDT, OpenLevel openLevel = OpenLevel::High);
    static float readBattery(float rTop, float rBottom);
    static bool bootButtonPressed();

    static int readPin(int pin);

private:
    static int readDebouncedPin(int pin);
};
