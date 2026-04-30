#include "UpdateManager.h"

void UpdateManager::performUpdate(const String& message) {
    // Parse JSON message
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        DEBUG_PRINTLN("Failed to parse JSON: " + String(error.c_str()));
        return;
    }

    if (doc["url"]) {
        String firmwareUrl = doc["url"];
        DEBUG_PRINTLN("Starting firmware update from: " + firmwareUrl);

        WiFiClient client;  // or WiFiClientSecure for HTTPS        
        HTTPUpdate httpUpdate;                
        
        HTTPUpdateResult result = httpUpdate.update(client, firmwareUrl);
        if (result == HTTP_UPDATE_OK) {
            DEBUG_PRINTLN("Firmware update successful. Restarting...");
            ESP.restart();
        } else {
            DEBUG_PRINTLN("Firmware update failed. Error: " + String(httpUpdate.getLastError()) + " - " + httpUpdate.getLastErrorString());
        }
    } else {
        DEBUG_PRINTLN("JSON message missing 'url' field");
    }
}