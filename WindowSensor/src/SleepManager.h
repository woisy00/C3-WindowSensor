#pragma once

#include <Arduino.h>

class SleepManager {
public:
    static void setupWake(uint8_t pin, unsigned long sleepTime);
    static void goToSleep(uint8_t pin, unsigned long sleepTime);
};
