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
}

void loop() {
    auto result = haManager.work();
    if (result.success) {
        uint8_t pin = static_cast<uint8_t>(result.windowState);
        SleepManager::goToSleep(pin, config.sleepTime);
    }
}
