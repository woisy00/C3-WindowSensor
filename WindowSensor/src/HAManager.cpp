#include "HAManager.h"
#include "BatteryUtil.h"
#include "version.h"

HAManager* HAManager::_instance = nullptr;

// --- Constructor ---

HAManager::HAManager(Config& cfg)
    : _config(cfg),
      _haDevice(),
      _mqtt(_wifiClient, _haDevice),
      _windowSensor("window"),
      _batterySensor("battery_voltage", HASensorNumber::PrecisionP2),
      _batteryPercent("battery_percent", HASensorNumber::PrecisionP0) {
    _instance = this;
}

// --- Setup ---

void HAManager::begin() {
    byte mac[6];
    WiFi.macAddress(mac);

    _haDevice.setUniqueId(mac, sizeof(mac));
    _haDevice.setName(_config.deviceName.c_str());
    _haDevice.setSoftwareVersion(VERSION);
    _haDevice.setModel("BWS-01");
    _haDevice.setManufacturer("bergermeier");

    _haDevice.enableSharedAvailability();

    // Window sensor
    _windowSensor.setDeviceClass("window");
    _windowSensor.setName("Window Sensor");

    // Battery voltage
    _batterySensor.setName("Battery voltage");
    _batterySensor.setDeviceClass("voltage");
    _batterySensor.setUnitOfMeasurement("V");

    // Battery %
    _batteryPercent.setName("Battery");
    _batteryPercent.setDeviceClass("battery");
    _batteryPercent.setUnitOfMeasurement("%");

    _mqtt.begin(
        _config.mqttHost.c_str(),
        _config.mqttUser.c_str(),
        _config.mqttPass.c_str()
    );
}

// --- Loop ---

void HAManager::loop() {
    _mqtt.loop();
}

// --- Publishers ---

void HAManager::publishWindow(bool open, bool force) {
    _windowSensor.setState(open, force);
}

void HAManager::publishBattery(float voltage, bool force) {
    _batterySensor.setValue(voltage, force);

    // Default: alkaline (can be made configurable later)
    float percent = BatteryUtil::voltageToPercent(voltage, BatteryType::ALKALINE);

    _batteryPercent.setValue(percent, force);
}