#include "Hardware.h"
#include "Utils.h"
#include <Arduino.h>
#include "driver/gpio.h"
#include "esp_adc_cal.h"
#include "WindowSensorDefines.h"

esp_adc_cal_characteristics_t adc_chars;

void Hardware::initPins() {
    // reed switch inputs
    pinMode(PIN_NO, INPUT);
    pinMode(PIN_NC, INPUT);
    // disable internal pullups
    gpio_pullup_dis((gpio_num_t)PIN_NO);
    gpio_pullup_dis((gpio_num_t)PIN_NC);
    gpio_pulldown_dis((gpio_num_t)PIN_NO);
    gpio_pulldown_dis((gpio_num_t)PIN_NC);    

    // battery ADC pin doesn't need mode for analogRead, but
    // setting it explicitly avoids accidental digital writes
    pinMode(PIN_BAT_ADC, INPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    // IDF holds wake pads through deep sleep; release only after restoring input/pull settings.
    gpio_hold_dis((gpio_num_t)PIN_NC);
    gpio_hold_dis((gpio_num_t)PIN_NO); // compatibility with older firmware's GPIO2 wake
    gpio_hold_dis((gpio_num_t)PIN_BUTTON);

    delay(50); // allow time for GPIO states to stabilize after power-up

}

Hardware::WindowState Hardware::readWindowState(ReedMode mode, OpenLevel openLevel) {
    delay(100); // allow settling - reduced back since GPIO hold prevents floating after deep sleep
    bool no = mode == ReedMode::SPDT ? readDebouncedPin(PIN_NO) : false;
    bool nc = readDebouncedPin(PIN_NC);
    DEBUG_PRINTLN("Read window state: NO=" + Utils::boolToString(no) + ", NC=" + Utils::boolToString(nc));
    WindowState state;
    state.no = no;
    state.nc = nc;
    state.mode = mode;
    state.openLevel = openLevel;
    return state;
}

int Hardware::readPin(int pin) {
    return readDebouncedPin(pin);
}

int Hardware::readDebouncedPin(int pin) {
    // Read pin multiple times and return stable value
    int lastValue = digitalRead(pin);
    int stableCount = 1;
    
    for (int i = 0; i < DEBOUNCE_SAMPLES - 1; i++) {
        delay(DEBOUNCE_DELAY_MS);
        int currentValue = digitalRead(pin);
        
        if (currentValue == lastValue) {
            stableCount++;
        } else {
            // State changed, reset counter
            lastValue = currentValue;
            stableCount = 1;
        }
        
        // If we have enough consecutive stable reads, return the value
        if (stableCount >= DEBOUNCE_SAMPLES) {
            return lastValue;
        }
    }
    
    return lastValue;
}

float Hardware::readBattery(float rTop, float rBottom) {
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);

    esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN_DB_12,
        ADC_WIDTH_BIT_12,
        1100,               // default Vref (used if no eFuse)
        &adc_chars
    );

    // discard first sample
    analogRead(PIN_BAT_ADC);
    delayMicroseconds(200);

    uint32_t sum = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        sum += analogRead(PIN_BAT_ADC);
        delayMicroseconds(200);
    }

    uint16_t raw = sum / BATTERY_SAMPLES;

    // convert to millivolts using calibration
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

    //float adc_voltage = (raw / 4095.0) * 2.5;

    if (rBottom <= 0.0) {
        DEBUG_PRINTLN("Warning: rBottom is zero or negative, defaulting to 1 kΩ");
        rBottom = 1.0;
    }
    float ratio = (rTop + rBottom) / rBottom;
    float battery_voltage = voltage_mv / 1000.0 * ratio;

    DEBUG_PRINTLN("Battery state: " + String(battery_voltage) + " (rTop=" + String(rTop) + "kΩ, rBottom=" + String(rBottom) + "kΩ)");
    return battery_voltage;
}

bool Hardware::bootButtonPressed() {
    delay(20);
    return digitalRead(PIN_BUTTON) == LOW;
}
