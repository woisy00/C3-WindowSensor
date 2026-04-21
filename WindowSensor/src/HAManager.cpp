#include "HAManager.h"
#include "version.h"

HAManager* HAManager::_instance = nullptr;

HAManager::HAManager(Config& cfg)
    : _config(cfg),
      _haDevice(),
      _mqtt(_wifiClient, _haDevice),
      _windowSensor("window"),
      _batterySensor("battery", HASensorNumber::PrecisionP2) {
    _instance = this;
}

void HAManager::begin() {
    byte mac[6];
    WiFi.macAddress(mac);
    _haDevice.setUniqueId(mac, sizeof(mac));
    _haDevice.setName(_config.deviceName.c_str());
    _haDevice.setSoftwareVersion(VERSION);
    _haDevice.setModel("BWS-01");
    _haDevice.setManufacturer("bergermeier");    

    _haDevice.enableSharedAvailability();

    _windowSensor.setDeviceClass("window");
    _windowSensor.setName("Window Sensor");

    _batterySensor.setName("Battery");
    _batterySensor.setDeviceClass("voltage");
    _batterySensor.setUnitOfMeasurement("V");
    
    _mqtt.begin(_config.mqttHost.c_str(), _config.mqttUser.c_str(), _config.mqttPass.c_str());
}

void HAManager::loop() {
    _mqtt.loop();
}

void HAManager::publishWindow(bool open, bool force) {
    _windowSensor.setState(open, force);
}

void HAManager::publishBattery(float voltage, bool force) {
    _batterySensor.setValue(voltage, force);
}
