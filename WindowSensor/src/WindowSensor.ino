// Uncomment to enable debug logging

#include "WindowSensorDefines.h"

#include "version.h"
#include "Utils.h"
#include "Config.h"
#include "WiFiConnector.h"
#include "Hardware.h"
#include "FirmwareManager.h"
#include "SleepManager.h"
#include "ButtonMonitor.h"
#include "BleReporter.h"

// Maintenance (config + OTA portal) is left automatically after this time to protect the battery.
static constexpr uint32_t MAINTENANCE_TIMEOUT_SEC = 10 * 60;

Config config;
WiFiConnector wifiConnector(config);
FirmwareManager firmwareManager(config);
SleepManager sleepManager(config);
BleReporter bleReporter(config);

// Blocking Wi-Fi portal (settings + firmware update). Connects to the stored network if possible,
// otherwise opens the access point. Always ends with a restart.
static void runMaintenance(bool configExists) {
    DEBUG_PRINTLN("Entering maintenance mode");
    bleReporter.shutdown(); // BLE stack must be down before Wi-Fi starts
    // first provisioning keeps the original no-timeout behaviour
    wifiConnector.connect(true, configExists ? MAINTENANCE_TIMEOUT_SEC : 0, &firmwareManager);
}

void setup() {
    DEBUG_INIT();

    DEBUG_PRINTLN("Starting Window Sensor " + String(VERSION));
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    DEBUG_PRINTLN("Wakeup cause: " + Utils::wakeupCauseToString(wakeup_cause));

    Hardware::initPins();
    ButtonMonitor::begin();

    bool configExists = config.load();
    firmwareManager.begin();
    // A button that is down at start (button wake, reset or boot) must be held 3 s continuously for
    // maintenance. A shorter press is released here and the normal report cycle continues.
    bool maintenanceRequested = ButtonMonitor::settle();
    if (config.portalRequested) {
        maintenanceRequested = true;
        if (!config.setPortalRequested(false)) {
            log_e("Could not clear the saved portal request; it may be repeated after reboot.");
        }
        DEBUG_PRINTLN("Portal requested by previous session");
    }
    DEBUG_PRINTLN("Maintenance requested");
    DEBUG_PRINTLN("  config: " + Utils::boolToString(configExists));
    DEBUG_PRINTLN("  button: " + Utils::boolToString(maintenanceRequested));

    if (configExists && !maintenanceRequested && config.blePortalScanEnabled) {
        maintenanceRequested = bleReporter.scanForPortalRequest(100);
        if (maintenanceRequested) DEBUG_PRINTLN("Remote BLE maintenance request received");
    }

    if (maintenanceRequested || !configExists) {
        runMaintenance(configExists);
    }
}

void loop() {
    Hardware::WindowState windowState;

    windowState = Hardware::readWindowState(config.reedMode, config.singleOpenLevel);
    const auto batteryReading = Hardware::readBattery(config.rTop, config.rBottom, config.batteryCorrectionFactor);
    if (!ButtonMonitor::maintenanceRequested() && !bleReporter.report(windowState, batteryReading.batteryVoltage)) {
        DEBUG_PRINTLN("BLE report failed or interrupted");
    }

    if (ButtonMonitor::maintenanceRequested()) {
        runMaintenance(true);
    }

    // returns without sleeping if the state changed or a maintenance hold completed
    sleepManager.goToSleep(windowState);

    if (ButtonMonitor::maintenanceRequested()) {
        runMaintenance(true);
    }
}
