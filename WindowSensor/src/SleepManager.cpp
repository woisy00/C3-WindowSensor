#include "SleepManager.h"
#include "Utils.h"
#include "Hardware.h"
#include <Arduino.h>
#include "driver/gpio.h"
#include "WindowSensorDefines.h"

void SleepManager::setupWake(uint8_t pin, unsigned long sleepTime) {
    if (sleepTime > 0) {
        DEBUG_PRINTLN("Setting wake timer to " + Utils::millisToHuman(sleepTime));
        esp_sleep_enable_timer_wakeup(sleepTime * 1000ULL);
    }

    DEBUG_PRINTLN("Going to sleep now until pin " + String(pin) + " is low");
    esp_deep_sleep_enable_gpio_wakeup((1ULL << pin), ESP_GPIO_WAKEUP_GPIO_LOW);
}

void SleepManager::goToSleep(uint8_t pin, unsigned long sleepTime) {
    setupWake(pin, sleepTime);
    delay(200);
    esp_deep_sleep_start();
}
