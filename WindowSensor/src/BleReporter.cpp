#include "BleReporter.h"
#include "BTHome.h"
#include "BatteryUtil.h"
#include "ButtonMonitor.h"
#include "FirmwareStrings.h"
#include "Utils.h"
#include "WindowSensorDefines.h"
#include <NimBLEDevice.h>
#include <string>

BleReporter::BleReporter(Config& cfg) : _config(cfg) {}

bool BleReporter::_start() {
    if (_stackInitialized) return true;
    DEBUG_PRINTLN("BLE initializing NimBLE (Wi-Fi remains off)");
    _stackInitialized = NimBLEDevice::init("");
    if (!_stackInitialized) log_e("BLE NimBLE initialization failed");
    return _stackInitialized;
}

bool BleReporter::scanForPortalRequest(uint32_t durationMs) {
    if (!_start()) return false;

    NimBLEScan* scanner = NimBLEDevice::getScan();
    if (!scanner) {
        log_e("BLE scan unavailable");
        shutdown();
        return false;
    }

    scanner->setActiveScan(false);
    scanner->setInterval(durationMs);
    scanner->setWindow(durationMs);
    NimBLEScanResults results = scanner->getResults(durationMs, false);
    const NimBLEUUID requestUuid(WAKEUP_SERVICE_UUID);
    const String ownDeviceId = Utils::BT_MAC(FirmwareStrings::Format::DeviceId);
    bool portalRequested = false;
    if (ownDeviceId.length() == 12) {
        for (int i = 0; i < results.getCount(); i++) {
            const NimBLEAdvertisedDevice* device = results.getDevice(i);
            if (!device) continue;
            const std::string serviceData = device->getServiceData(requestUuid);
            if (serviceData == ownDeviceId.c_str()) {
                portalRequested = true;
                break;
            }
        }
    } else {
        log_e("BLE device ID unavailable; ignoring portal request scan");
    }
    DEBUG_PRINTLN("BLE passive startup scan: " + String(durationMs) + " ms, portal request " +
                  (portalRequested ? "matched" : "not found"));
    shutdown();
    return portalRequested;
}

bool BleReporter::_advertise(const uint8_t* data, size_t len) {
    DEBUG_PRINTLN("BLE configuring advertisement: " + String(len) + " bytes, interval " +
                  String(ADV_INTERVAL_MIN * 0.625f) + "-" +
                  String(ADV_INTERVAL_MAX * 0.625f) + " ms, channels 37/38/39");
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    if (!advertising) {
        log_e("BLE advertising unavailable");
        return false;
    }

    NimBLEAdvertisementData advData;
    if (!advData.addData(data, len) ||
        !advertising->setConnectableMode(BLE_GAP_CONN_MODE_NON) ||
        !advertising->setDiscoverableMode(BLE_GAP_DISC_MODE_NON) ||
        !advertising->setAdvertisementData(advData)) {
        log_e("BLE advertisement configuration failed");
        return false;
    }
    advertising->setMinInterval(ADV_INTERVAL_MIN);
    advertising->setMaxInterval(ADV_INTERVAL_MAX);
    _advertising = advertising->start();
    if (!_advertising) log_e("BLE advertising start failed");
    return _advertising;
}

bool BleReporter::report(const Hardware::WindowState& state, float batteryVoltage) {
    uint32_t reportStart = millis();
    float percent = BatteryUtil::voltageToPercent(batteryVoltage, BatteryType::ALKALINE);
    float mv = batteryVoltage * 1000.0f;
    uint16_t voltageMv = mv < 0 ? 0 : (mv > 65535.0f ? 65535 : (uint16_t)(mv + 0.5f));
    DEBUG_PRINTLN("BLE report: window=" + String(state.isOpen() ? "OPEN" : "CLOSED") +
                  ", NO=" + String(state.no) + ", NC=" + String(state.nc) +
                  ", battery=" + String(batteryVoltage, 3) + " V (" +
                  String(percent, 0) + "%)");

    uint8_t adv[BTHome::MAX_ADV_LEN];
    size_t len = BTHome::buildAdvertisement(adv, sizeof(adv), state.isOpen(), (uint8_t)(percent + 0.5f),
                                            voltageMv, _config.deviceName.c_str());
    if (len == 0) {
        log_e("BLE advertisement payload too large");
        return false;
    }
#if defined(DEBUG)
    String payload;
    payload.reserve(len * 3);
    for (size_t i = 0; i < len; i++) {
        if (i) payload += ' ';
        if (adv[i] < 0x10) payload += '0';
        payload += String(adv[i], HEX);
    }
    DEBUG_PRINTLN("BLE BTHome payload (" + String(len) + " bytes): " + payload);
#endif

    bool ok = _start() && _advertise(adv, len);
    if (ok) {
        DEBUG_PRINTLN("BLE advertising burst started for " + String(BURST_MS) + " ms");
        // a maintenance hold during the burst aborts it
        ok = !ButtonMonitor::delayServicing(BURST_MS);
        DEBUG_PRINTLN(ok ? "BLE advertising burst completed (reception is not acknowledged)" :
                          "BLE advertising interrupted by maintenance button hold");
    } else {
        DEBUG_PRINTLN("BLE advertising could not start");
    }
    shutdown();
    DEBUG_PRINTLN("BLE report finished: " + String(ok ? "completed" : "failed/interrupted") +
                  ", elapsed=" + String(millis() - reportStart) + " ms");
    return ok;
}

void BleReporter::shutdown() {
    if (_advertising) {
        DEBUG_PRINTLN("BLE stopping advertising");
        NimBLEDevice::stopAdvertising();
        _advertising = false;
    }
    if (_stackInitialized) {
        DEBUG_PRINTLN("BLE deinitializing NimBLE");
        if (!NimBLEDevice::deinit(true)) log_e("BLE NimBLE deinitialization failed");
        _stackInitialized = false;
    }
}
