#pragma once

#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include "WindowSensorDefines.h"

class UpdateManager {
public:
    static void performUpdate(const String& message);
};