// Uncomment to enable debug logging
// #define DEBUG 1

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

void setup() {
#ifdef DEBUG
    Serial.begin(115200);
#endif

    Hardware::initPins();

    bool configExists = config.load();
    bool portalRequested = Hardware::bootButtonPressed();
#ifdef DEBUG
    Utils::logMsg("Portal requested");
    Utils::logMsg("  config: " + Utils::boolToString(configExists));
    Utils::logMsg("  button: " + Utils::boolToString(portalRequested));
#endif

    wifiConnector.connect(!configExists || portalRequested);

    haManager.begin();

    windowOpen = Hardware::readWindowState();
    float battery = Hardware::readBattery(config.rTop, config.rBottom);

    haManager.publishWindow(windowOpen, true);
    haManager.publishBattery(battery, true);    
#ifdef DEBUG
    Utils::logMsg("Sending state to HA: window: " + Utils::boolToString(windowOpen) + ", batt: " + String(battery));
#endif

    wakeDeadline = millis() + config.loopTime;
}

void loop() {
    haManager.loop();

    //bool windowOpen = Hardware::readWindowState();
    //float battery = Hardware::readBattery(config.rTop, config.rBottom);

    //haManager.publishWindow(windowOpen, false);
    //haManager.publishBattery(battery, false);  
      
    if (millis() >= wakeDeadline) {
        SleepManager::goToSleep(windowOpen, config.sleepTime);
    }

    delay(5);
}
