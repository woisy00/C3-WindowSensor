#pragma once
#include <Arduino.h>

class Utils {
public:
    static unsigned long parseDuration(const String& input);
    static String millisToHuman(unsigned long ms);
    static String boolToString(bool b);

#ifdef DEBUG
    static void logMsg(const String& message);
#endif
};
