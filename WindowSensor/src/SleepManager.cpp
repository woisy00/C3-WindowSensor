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
    Hardware::WindowState currentState = Hardware::readWindowState(_config.reedMode, _config.singleOpenLevel);
       
    // Only proceed with sleep if the current state matches the expected state
    if (!currentState.matches(windowState)) {
        DEBUG_PRINTLN("State mismatch: expected " + String(windowState.isOpen()) + " but got " + String(currentState.isOpen()) + ". Skipping sleep.");
        return;
    }
    
    unsigned long sleepTime = _config.sleepTime;
    const auto plan = ReedLogic::wakePlan(currentState);
    if (plan.retry) {
        log_e("Invalid SPDT state: GPIO1 and GPIO2 have the same level. Retrying after 1 sec.");
        sleepTime = 1000ULL;
    }

    // Clear hardware wake left by an aborted sleep attempt.
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    gpio_deep_sleep_wakeup_disable((gpio_num_t)Hardware::PIN_NC);
    gpio_deep_sleep_wakeup_disable((gpio_num_t)Hardware::PIN_BUTTON);

    esp_err_t err = esp_deep_sleep_enable_gpio_wakeup(1ULL << Hardware::PIN_BUTTON, ESP_GPIO_WAKEUP_GPIO_LOW);
    if (err != ESP_OK) {
        log_e("Failed to enable button wakeup: %s", esp_err_to_name(err));
        sleepTime = 1000ULL;
    }
    if (plan.reedEnabled) {
        // IDF 4.4 accumulates per-pin wake masks: this does not overwrite GPIO5's LOW wake.
        err = esp_deep_sleep_enable_gpio_wakeup(1ULL << Hardware::PIN_NC,
                plan.reedHigh ? ESP_GPIO_WAKEUP_GPIO_HIGH : ESP_GPIO_WAKEUP_GPIO_LOW);
        if (err != ESP_OK) {
            log_e("Failed to enable GPIO1 wakeup: %s", esp_err_to_name(err));
            sleepTime = 1000ULL;
        }
    }

    // Arduino 2's prebuilt IDF 4.4 auto-enables pulls at sleep entry (HIGH wake => pulldown).
    // Hold GPIO1's input/no-pull pad configuration BEFORE that step, preserving external bias.
    // Input signals still change while held. GPIO5 keeps IDF's automatic sleep pull-up.
    err = gpio_set_pull_mode((gpio_num_t)Hardware::PIN_NC, GPIO_FLOATING);
    if (err == ESP_OK) err = gpio_hold_en((gpio_num_t)Hardware::PIN_NC);
    if (err != ESP_OK) {
        log_e("Cannot preserve GPIO1 external bias: %s. Skipping sleep.", esp_err_to_name(err));
        return;
    }

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    if (sleepTime > 0) {
        DEBUG_PRINTLN("Setting wake timer to " + Utils::millisToHuman(sleepTime));
        err = esp_sleep_enable_timer_wakeup(sleepTime * 1000ULL);
        if (err != ESP_OK) {
            log_e("Failed to enable wake timer: %s. Skipping sleep.", esp_err_to_name(err));
            gpio_hold_dis((gpio_num_t)Hardware::PIN_NC);
            return;
        }
    }

    delay(50);
    // Final guard after wake/pull setup. In single mode GPIO2 is neither sampled nor compared.
    const auto finalState = Hardware::readWindowState(_config.reedMode, _config.singleOpenLevel);
    if (!finalState.matches(currentState) || digitalRead(Hardware::PIN_NC) != currentState.nc ||
        ButtonMonitor::holdPending() || ButtonMonitor::maintenanceRequested()) {
        gpio_hold_dis((gpio_num_t)Hardware::PIN_NC);
        DEBUG_PRINTLN("State or button changed just before sleep. Skipping sleep.");
        return;
    }
    esp_deep_sleep_start();
}
