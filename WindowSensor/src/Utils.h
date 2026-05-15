#pragma once
#include <Arduino.h>
#include <esp_sleep.h>

class Utils {
public:
    static unsigned long parseDuration(const String& input);
    static String millisToHuman(unsigned long ms);
    static String boolToString(bool b);
    static String wakeupCauseToString(esp_sleep_wakeup_cause_t cause);
};
