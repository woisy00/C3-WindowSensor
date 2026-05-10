#pragma once

#include <Arduino.h>
#include "Config.h"

class SleepManager {
public:
    explicit SleepManager(Config& cfg);
    
    void goToSleep();
private:
    Config& _config;
    void setupWake();
};
