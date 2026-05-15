#include "SleepManager.h"
#include "Utils.h"
#include "Hardware.h"
#include <Arduino.h>
#include "driver/gpio.h"
#include "WindowSensorDefines.h"

SleepManager::SleepManager(Config& cfg) : _config(cfg) {}


void SleepManager::goToSleep(Hardware::WindowState windowState) {
    Hardware::WindowState currentState = Hardware::readWindowState(); // read again to ensure we have the latest state before sleeping
       
    // Only proceed with sleep if the current state matches the expected state
    if (!currentState.matches(windowState)) {
        DEBUG_PRINTLN("State mismatch: expected " + String(windowState.isOpen()) + " but got " + String(currentState.isOpen()) + ". Skipping sleep.");
        return;
    }
    
    unsigned long sleepTime = _config.sleepTime;

    if (currentState.no == LOW && currentState.nc == HIGH) {
        // Window is closed, wake on NO pin
        DEBUG_PRINTLN("Window is closed, setting wake on NC pin");
        esp_deep_sleep_enable_gpio_wakeup((1ULL << Hardware::PIN_NC), ESP_GPIO_WAKEUP_GPIO_LOW);
    } else if (currentState.no == HIGH && currentState.nc == LOW) {
        // Window is open, wake on NC pin
        DEBUG_PRINTLN("Window is open, setting wake on NO pin");
        esp_deep_sleep_enable_gpio_wakeup((1ULL << Hardware::PIN_NO), ESP_GPIO_WAKEUP_GPIO_LOW);
    } else {
        // Both pins are in the same state - this shouldn't happen with a proper SPDT switch
        DEBUG_PRINTLN("Warning: Both NO and NC pins are in the same state. Skipping wake pins and waking after 1 sec.");
        sleepTime = 1000ULL;
    }

    if (sleepTime > 0) {
        DEBUG_PRINTLN("Setting wake timer to " + Utils::millisToHuman(sleepTime));
        esp_sleep_enable_timer_wakeup(sleepTime * 1000ULL);
    }

    delay(50);    
    esp_deep_sleep_start();
}
