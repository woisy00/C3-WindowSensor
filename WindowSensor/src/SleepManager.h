#pragma once

#include <Arduino.h>
#include "Config.h"
#include "Hardware.h"

class SleepManager {
public:
    explicit SleepManager(Config& cfg);

    // Returns without sleeping when the window state changed meanwhile or a maintenance
    // button hold was detected (check ButtonMonitor::maintenanceRequested()).
    void goToSleep(Hardware::WindowState windowState);
private:
    Config& _config;
};
