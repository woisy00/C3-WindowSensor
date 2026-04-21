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

void Utils::logMsg(const String& message) {
    Serial.println(message);
    Serial.flush();
}
