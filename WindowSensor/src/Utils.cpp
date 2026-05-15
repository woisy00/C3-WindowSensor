#include "Utils.h"

unsigned long Utils::parseDuration(const String& input) {
    String s = input;
    s.toLowerCase();
    unsigned long total = 0;
    int number = 0;

    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (isdigit(c)) {
            number = number * 10 + (c - '0');
        } else {
            switch (c) {
                case 'h': total += number * 3600UL * 1000UL; break;
                case 'm': total += number * 60UL * 1000UL; break;
                case 's': total += number * 1000UL; break;
                default: break;
            }
            number = 0;
        }
    }
    if (number > 0) total += number * 1000UL;
    return total;
}

String Utils::millisToHuman(unsigned long ms) {
    unsigned long seconds = ms / 1000UL;
    unsigned long hours = seconds / 3600UL;
    seconds %= 3600UL;
    unsigned long minutes = seconds / 60UL;
    seconds %= 60UL;

    String result = "";
    if (hours) result += String(hours) + "h";
    if (minutes) result += String(minutes) + "m";
    if (seconds || result == "") result += String(seconds) + "s";
    return result;
}

String Utils::boolToString(bool b) {
    return b ? "true" : "false";
}

String Utils::wakeupCauseToString(esp_sleep_wakeup_cause_t cause) {
    switch(cause) {
        case ESP_SLEEP_WAKEUP_UNDEFINED: return "UNDEFINED";
        case ESP_SLEEP_WAKEUP_ALL: return "ALL";
        case ESP_SLEEP_WAKEUP_EXT0: return "EXT0";
        case ESP_SLEEP_WAKEUP_EXT1: return "EXT1";
        case ESP_SLEEP_WAKEUP_TIMER: return "TIMER";
        case ESP_SLEEP_WAKEUP_TOUCHPAD: return "TOUCHPAD";
        case ESP_SLEEP_WAKEUP_ULP: return "ULP";
        case ESP_SLEEP_WAKEUP_GPIO: return "GPIO";
        case ESP_SLEEP_WAKEUP_UART: return "UART";
        case ESP_SLEEP_WAKEUP_WIFI: return "WIFI";
        case ESP_SLEEP_WAKEUP_COCPU: return "COCPU";
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG: return "COCPU_TRAP_TRIG";
        case ESP_SLEEP_WAKEUP_BT: return "BT";
        default: return "UNKNOWN";
    }
}