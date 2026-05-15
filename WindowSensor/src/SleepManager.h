#pragma once

#include <Arduino.h>
#include "Config.h"
#include "Hardware.h"

class SleepManager {
public:
    explicit SleepManager(Config& cfg);
    
    void goToSleep(Hardware::WindowState windowState);
private:
    Config& _config;
};
