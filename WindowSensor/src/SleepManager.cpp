#include "SleepManager.h"
#include "Utils.h"
#include "Hardware.h"
#include <Arduino.h>

void SleepManager::setupWake(bool windowOpen, unsigned long sleepTime) {
    if (sleepTime > 0) {
#ifdef DEBUG
        Utils::logMsg("Setting wake timer to " + Utils::millisToHuman(sleepTime));
#endif
        esp_sleep_enable_timer_wakeup(sleepTime * 1000ULL);
    }

    if (windowOpen) {
#ifdef DEBUG
        Utils::logMsg("Going to sleept now until NC_PIN is low");
#endif
        esp_deep_sleep_enable_gpio_wakeup((1ULL << Hardware::PIN_NC), ESP_GPIO_WAKEUP_GPIO_LOW);
    } else {
#ifdef DEBUG
        Utils::logMsg("Going to sleept now until NO_PIN is low");
#endif
        esp_deep_sleep_enable_gpio_wakeup((1ULL << Hardware::PIN_NO), ESP_GPIO_WAKEUP_GPIO_LOW);
    }
}

void SleepManager::goToSleep(bool windowOpen, unsigned long sleepTime) {
    setupWake(windowOpen, sleepTime);
    delay(200);
    esp_deep_sleep_start();
}
