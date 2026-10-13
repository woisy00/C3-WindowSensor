#include "SleepManager.h"
#include "Utils.h"
#include "Hardware.h"
#include "ButtonMonitor.h"
#include <Arduino.h>
#include "driver/gpio.h"
#include "WindowSensorDefines.h"

SleepManager::SleepManager(Config& cfg) : _config(cfg) {}


void SleepManager::goToSleep(Hardware::WindowState windowState) {
    // never sleep while a button hold is still being evaluated: a low level would wake us right away
    if (ButtonMonitor::settle()) {
        DEBUG_PRINTLN("Maintenance hold detected. Skipping sleep.");
        return;
    }
    Hardware::WindowState currentState = Hardware::readWindowState(); // read again to ensure we have the latest state before sleeping
       
    // Only proceed with sleep if the current state matches the expected state
    if (!currentState.matches(windowState)) {
        DEBUG_PRINTLN("State mismatch: expected " + String(windowState.isOpen()) + " but got " + String(currentState.isOpen()) + ". Skipping sleep.");
        return;
    }
    
    unsigned long sleepTime = _config.sleepTime;
    uint64_t wakeMask = 0;

    if (currentState.no == LOW && currentState.nc == HIGH) {
        // Wake when the currently-high NC contact goes low.
        DEBUG_PRINTLN("Window is closed, setting wake on NC pin");
        wakeMask |= (1ULL << Hardware::PIN_NC);
    } else if (currentState.no == HIGH && currentState.nc == LOW) {
        // Wake when the currently-high NO contact goes low.
        DEBUG_PRINTLN("Window is open, setting wake on NO pin");
        wakeMask |= (1ULL << Hardware::PIN_NO);
    } else {
        // Both pins are in the same state - this shouldn't happen with a proper SPDT switch
        DEBUG_PRINTLN("Warning: Both NO and NC pins are in the same state. Skipping wake pins and waking after 1 sec.");
        sleepTime = 1000ULL;
    }

    // Button (active low) wakes alongside the currently-high reed contact. IDF enables the internal
    // pull-up of low-level wake pins during deep sleep, so the button idles high without external parts.
    wakeMask |= (1ULL << Hardware::PIN_BUTTON);

    for (int pin = 0; pin < 64; pin++) {
        if ((wakeMask >> pin) & 1ULL) {
            if (!esp_sleep_is_valid_wakeup_gpio((gpio_num_t)pin)) {
                log_e("GPIO%d is not a valid deep sleep wake pin", pin);
                wakeMask &= ~(1ULL << pin);
                sleepTime = 1000ULL;
            }
        }
    }
    if (wakeMask != 0) {
        esp_err_t err = esp_deep_sleep_enable_gpio_wakeup(wakeMask, ESP_GPIO_WAKEUP_GPIO_LOW);
        if (err != ESP_OK) {
            log_e("Failed to enable GPIO wakeup: %s", esp_err_to_name(err));
            sleepTime = 1000ULL; // fall back to polling rather than sleeping deaf
        }
    }

    if (sleepTime > 0) {
        DEBUG_PRINTLN("Setting wake timer to " + Utils::millisToHuman(sleepTime));
        esp_sleep_enable_timer_wakeup(sleepTime * 1000ULL);
    }

    delay(50);    
    esp_deep_sleep_start();
}
