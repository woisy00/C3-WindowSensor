#pragma once

#include <stdint.h>
#include <stddef.h>
#include "version.h"

// BTHome v2 (https://bthome.io/format/) advertisement builder. Pure logic, no Arduino/IDF dependency.
namespace BTHome {
    static constexpr uint8_t OBJ_BATTERY_PERCENT = 0x01; // uint8, 1 %
    static constexpr uint8_t OBJ_VOLTAGE_MV = 0x0C;      // uint16 LE, 0.001 V
    static constexpr uint8_t OBJ_WINDOW = 0x2D;          // uint8, 0 = closed, 1 = open
    static constexpr uint8_t OBJ_TEXT = 0x53;
    static constexpr char FIRMWARE_VERSION_TEXT[] = VERSION;
    static constexpr size_t FIRMWARE_VERSION_LEN = sizeof(FIRMWARE_VERSION_TEXT) - 1;
    // bit 2: trigger based device, bits 5-7: BTHome version 2, bit 0 = 0: unencrypted
    static constexpr uint8_t DEVICE_INFO = 0x44;
    static constexpr size_t MAX_ADV_LEN = 31;
    static constexpr size_t SERVICE_DATA_LEN = 1 + 2 + 1 + 2 + 3 + 2 + 2 + FIRMWARE_VERSION_LEN;
    static_assert(FIRMWARE_VERSION_LEN > 0 && 3 + 1 + SERVICE_DATA_LEN <= MAX_ADV_LEN,
                  "BTHome firmware text must fit alongside the sensor readings");
    static constexpr size_t MAX_NAME_LEN = MAX_ADV_LEN > 3 + 1 + SERVICE_DATA_LEN + 2 ?
                                          MAX_ADV_LEN - (3 + 1 + SERVICE_DATA_LEN + 2) : 0;

    // Builds a complete legacy advertising payload (flags + service data [+ name]).
    // Objects are written sorted by object id as required by the spec.
    // Returns the payload length, or 0 if it does not fit into cap/31 bytes.
    inline size_t buildAdvertisement(uint8_t* out, size_t cap, bool windowOpen, uint8_t batteryPercent,
                                     uint16_t voltageMv, const char* name) {
        const size_t serviceDataLen = SERVICE_DATA_LEN;
        size_t nameLen = 0;
        while (name && name[nameLen] && nameLen < MAX_NAME_LEN) nameLen++;
        size_t total = 3 + 1 + serviceDataLen + (nameLen ? 2 + nameLen : 0);
        if (!out || total > MAX_ADV_LEN || total > cap) return 0;
        if (batteryPercent > 100) batteryPercent = 100;

        size_t i = 0;
        out[i++] = 0x02; out[i++] = 0x01; out[i++] = 0x06; // flags: LE general discoverable, no BR/EDR
        out[i++] = (uint8_t)serviceDataLen;
        out[i++] = 0x16;                                   // service data, 16-bit UUID
        out[i++] = 0xD2; out[i++] = 0xFC;                  // 0xFCD2 little endian
        out[i++] = DEVICE_INFO;
        out[i++] = OBJ_BATTERY_PERCENT; out[i++] = batteryPercent;
        out[i++] = OBJ_VOLTAGE_MV; out[i++] = (uint8_t)(voltageMv & 0xFF); out[i++] = (uint8_t)(voltageMv >> 8);
        out[i++] = OBJ_WINDOW; out[i++] = windowOpen ? 1 : 0;
        out[i++] = OBJ_TEXT;
        out[i++] = (uint8_t)FIRMWARE_VERSION_LEN;
        for (size_t offset = 0; offset < FIRMWARE_VERSION_LEN; offset++) {
            out[i++] = (uint8_t)FIRMWARE_VERSION_TEXT[offset];
        }
        if (nameLen) {
            out[i++] = (uint8_t)(nameLen + 1);
            out[i++] = name[nameLen] ? 0x08 : 0x09;
            for (size_t n = 0; n < nameLen; n++) out[i++] = (uint8_t)name[n];
        }
        return i;
    }
}
