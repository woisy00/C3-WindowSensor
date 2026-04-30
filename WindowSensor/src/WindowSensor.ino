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

bool windowOpen = false;
unsigned long wakeDeadline = 0;
float battery = 0.0;

void setup() {
    DEBUG_INIT();

    Hardware::initPins();

    bool configExists = config.load();
    bool portalRequested = Hardware::bootButtonPressed();
    DEBUG_PRINTLN("Portal requested");
    DEBUG_PRINTLN("  config: " + Utils::boolToString(configExists));
    DEBUG_PRINTLN("  button: " + Utils::boolToString(portalRequested));

    wifiConnector.connect(!configExists || portalRequested);

    haManager.begin();    

    windowOpen = Hardware::readWindowState();
    battery = Hardware::readBattery(config.rTop, config.rBottom);

    //haManager.publishWindow(windowOpen, true);
    //haManager.publishBattery(battery, true);    
    DEBUG_PRINTLN("Sending state to HA: window: " + Utils::boolToString(windowOpen) + ", batt: " + String(battery));

    wakeDeadline = millis() + config.loopTime;
}

void loop() {
    haManager.loop();

    #ifndef DEBUG
    windowOpen = Hardware::readWindowState();
    //float battery = Hardware::readBattery(config.rTop, config.rBottom);
    haManager.publishWindow(windowOpen, false);
    haManager.publishBattery(battery, false);  
    if (millis() >= wakeDeadline) {
        SleepManager::goToSleep(windowOpen, config.sleepTime);
    }
    #endif

    delay(5);
}
