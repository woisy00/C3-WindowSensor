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

    DEBUG_PRINTLN("Going to sleep now until pin " + String(pin) + " is low or button is pressed");
    esp_deep_sleep_enable_gpio_wakeup((1ULL << pin) | (1ULL << Hardware::PIN_BUTTON), ESP_GPIO_WAKEUP_GPIO_LOW);
}

void SleepManager::goToSleep(uint8_t pin, unsigned long sleepTime) {
    setupWake(pin, sleepTime);
    // Hold the switch pins during deep sleep to maintain state
    gpio_hold_en((gpio_num_t)Hardware::PIN_NO);
    gpio_hold_en((gpio_num_t)Hardware::PIN_NC);
    delay(200);
    esp_deep_sleep_start();
}
