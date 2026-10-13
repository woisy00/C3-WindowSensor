#include "BleReporter.h"
#include "BTHome.h"
#include "BatteryUtil.h"
#include "ButtonMonitor.h"
#include "WindowSensorDefines.h"
#include "esp_bt.h"
#include "esp_err.h"
#include <atomic>
#include <string.h>

// HCI opcodes (OGF << 10 | OCF)
static constexpr uint16_t OP_RESET = 0x0C03;
static constexpr uint16_t OP_LE_SET_ADV_PARAMS = 0x2006;
static constexpr uint16_t OP_LE_SET_ADV_DATA = 0x2008;
static constexpr uint16_t OP_LE_SET_ADV_ENABLE = 0x200A;

// Written by the controller callback, read by the main task
static std::atomic<bool> s_cmdDone{false};
static uint16_t s_cmdOpcode = 0;
static uint8_t s_cmdStatus = 0xFF;

static void hostSendAvailable() {}

static int hostReceive(uint8_t* data, uint16_t len) {
    // HCI event packet: 0x04, code, plen, ...  Command Complete: numPkts, opcode(2), status
    if (len >= 7 && data[0] == 0x04 && data[1] == 0x0E) {
        s_cmdOpcode = (uint16_t)(data[4] | (data[5] << 8));
        s_cmdStatus = data[6];
        s_cmdDone.store(true, std::memory_order_release);
    } else if (len >= 7 && data[0] == 0x04 && data[1] == 0x0F) {
        // Command Status: status, numPkts, opcode(2)
        s_cmdOpcode = (uint16_t)(data[5] | (data[6] << 8));
        s_cmdStatus = data[3];
        s_cmdDone.store(true, std::memory_order_release);
    }
    return 0;
}

static const esp_vhci_host_callback_t s_hostCallback = { hostSendAvailable, hostReceive };

static bool check(esp_err_t err, const char* what) {
    if (err != ESP_OK) {
        log_e("BLE %s failed: %s", what, esp_err_to_name(err));
        return false;
    }
    return true;
}

// Sends one HCI command and waits for its completion event, so commands never overlap.
static bool sendCommand(uint16_t opcode, const uint8_t* params, uint8_t paramLen) {
    DEBUG_PRINTLN("BLE HCI send: opcode=0x" + String(opcode, HEX) + ", parameter bytes=" + String(paramLen));
    uint8_t pkt[4 + 32];
    pkt[0] = 0x01;
    pkt[1] = (uint8_t)(opcode & 0xFF);
    pkt[2] = (uint8_t)(opcode >> 8);
    pkt[3] = paramLen;
    if (paramLen) memcpy(pkt + 4, params, paramLen);

    uint32_t start = millis();
    while (!esp_vhci_host_check_send_available()) {
        if ((uint32_t)(millis() - start) > 500) {
            log_e("BLE controller not ready for opcode 0x%04X", opcode);
            return false;
        }
        delay(2);
    }
    s_cmdDone.store(false, std::memory_order_release);
    esp_vhci_host_send_packet(pkt, 4 + paramLen);

    start = millis();
    while (!s_cmdDone.load(std::memory_order_acquire)) {
        if ((uint32_t)(millis() - start) > 1000) {
            log_e("BLE timeout for opcode 0x%04X", opcode);
            return false;
        }
        delay(2);
    }
    if (s_cmdOpcode != opcode || s_cmdStatus != 0) {
        log_e("BLE opcode 0x%04X received opcode 0x%04X status 0x%02X", opcode, s_cmdOpcode, s_cmdStatus);
        return false;
    }
    DEBUG_PRINTLN("BLE HCI complete: opcode=0x" + String(opcode, HEX) +
                  ", status=0x" + String(s_cmdStatus, HEX) +
                  ", completion wait=" + String(millis() - start) + " ms");
    return true;
}

BleReporter::BleReporter(Config& cfg) : _config(cfg) {}

bool BleReporter::_start() {
    DEBUG_PRINTLN("BLE initializing controller (Wi-Fi remains off)");
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (!check(esp_bt_controller_init(&cfg), "controller init")) return false;
    _controllerInit = true;
    DEBUG_PRINTLN("BLE controller initialized");
    if (!check(esp_bt_controller_enable(ESP_BT_MODE_BLE), "controller enable")) return false;
    _controllerEnabled = true;
    DEBUG_PRINTLN("BLE controller enabled; registering HCI callback");
    return check(esp_vhci_host_register_callback(&s_hostCallback), "register HCI callback");
}

bool BleReporter::_advertise(const uint8_t* data, size_t len) {
    DEBUG_PRINTLN("BLE configuring advertisement: " + String(len) + " bytes, interval " +
                  String(ADV_INTERVAL_MIN * 0.625f) + "-" +
                  String(ADV_INTERVAL_MAX * 0.625f) + " ms, channels 37/38/39");
    if (!sendCommand(OP_RESET, nullptr, 0)) return false;

    uint8_t params[15] = {};
    params[0] = ADV_INTERVAL_MIN & 0xFF; params[1] = ADV_INTERVAL_MIN >> 8;
    params[2] = ADV_INTERVAL_MAX & 0xFF; params[3] = ADV_INTERVAL_MAX >> 8;
    params[4] = 0x03; // ADV_NONCONN_IND (BTHome is broadcast only)
    params[5] = 0x00; // own address type: public
    params[6] = 0x00; // peer address type (unused)
    // params[7..12] peer address (unused)
    params[13] = 0x07; // all three advertising channels
    params[14] = 0x00; // no filter policy
    if (!sendCommand(OP_LE_SET_ADV_PARAMS, params, sizeof(params))) return false;

    // advertising is only enabled after the data command has completed
    uint8_t advData[32] = {};
    advData[0] = (uint8_t)len;
    memcpy(advData + 1, data, len);
    if (!sendCommand(OP_LE_SET_ADV_DATA, advData, sizeof(advData))) return false;

    uint8_t enable = 1;
    if (!sendCommand(OP_LE_SET_ADV_ENABLE, &enable, 1)) return false;
    _advertising = true;
    return true;
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
        uint8_t disable = 0;
        sendCommand(OP_LE_SET_ADV_ENABLE, &disable, 1);
        _advertising = false;
    }
    if (_controllerEnabled) {
        DEBUG_PRINTLN("BLE disabling controller");
        check(esp_bt_controller_disable(), "controller disable");
        _controllerEnabled = false;
    }
    if (_controllerInit) {
        DEBUG_PRINTLN("BLE deinitializing controller");
        check(esp_bt_controller_deinit(), "controller deinit");
        _controllerInit = false;
    }
}
