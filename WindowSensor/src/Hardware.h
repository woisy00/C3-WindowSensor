#pragma once

#include <Arduino.h>
#include <array>
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

    struct BatteryReading {
        std::array<uint16_t, BATTERY_SAMPLES> samples{};
        uint16_t discardedRaw = 0;
        uint16_t raw = 0;
        uint32_t adcMillivolts = 0;
        float rTop = 0.0f;
        float rBottom = 0.0f;
        bool rBottomFallbackUsed = false;
        float dividerRatio = 0.0f;
        float correctionFactor = 1.0f;
        float batteryVoltage = 0.0f;
    };

    static void initPins();
    static WindowState readWindowState(ReedMode mode = ReedMode::SPDT, OpenLevel openLevel = OpenLevel::High);
    static BatteryReading readBattery(float rTop, float rBottom, float correctionFactor = 1.0f);
    static bool bootButtonPressed();

    static int readPin(int pin);

private:
    static int readDebouncedPin(int pin);
};
