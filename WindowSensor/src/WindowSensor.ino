// Uncomment to enable debug logging

#include "WindowSensorDefines.h"

#include "version.h"
#include "Utils.h"
#include "Config.h"
#include "WiFiConnector.h"
#include "Hardware.h"
#include "HAManager.h"
#include "SleepManager.h"

Config config;
WiFiConnector wifiConnector(config);
HAManager haManager(config);
SleepManager sleepManager(config);

void setup() {
    DEBUG_INIT();

    DEBUG_PRINTLN("Starting Window Sensor " + String(VERSION));
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    DEBUG_PRINTLN("Wakeup cause: " + Utils::wakeupCauseToString(wakeup_cause));

    Hardware::initPins();

    bool configExists = config.load();
    bool portalRequested = Hardware::bootButtonPressed();
    DEBUG_PRINTLN("Portal requested");
    DEBUG_PRINTLN("  config: " + Utils::boolToString(configExists));
    DEBUG_PRINTLN("  button: " + Utils::boolToString(portalRequested));

    wifiConnector.connect(!configExists || portalRequested);

    haManager.begin();    
}

void loop() {
    haManager.work();
    sleepManager.goToSleep();
}
