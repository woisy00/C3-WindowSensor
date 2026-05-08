#include "Hardware.h"
#include "Utils.h"
#include <Arduino.h>
#include "driver/gpio.h"
#include "esp_adc_cal.h"
#include "WindowSensorDefines.h"

esp_adc_cal_characteristics_t adc_chars;

void Hardware::initPins() {
    // Release GPIO hold after deep sleep wake-up
    gpio_hold_dis((gpio_num_t)PIN_NO);
    gpio_hold_dis((gpio_num_t)PIN_NC);
    // reed switch inputs
    pinMode(PIN_NO, INPUT);
    pinMode(PIN_NC, INPUT);
    // battery ADC pin doesn't need mode for analogRead, but
    // setting it explicitly avoids accidental digital writes
    pinMode(PIN_BAT_ADC, INPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
}

Hardware::WindowState Hardware::readWindowState() {
    delay(10); // allow settling - reduced back since GPIO hold prevents floating after deep sleep
    bool no = digitalRead(PIN_NO);
    bool nc = digitalRead(PIN_NC);
    DEBUG_PRINTLN("Read window state: NO=" + Utils::boolToString(no) + ", NC=" + Utils::boolToString(nc));
    // read the NO and NC pins and determine window state
    // Assuming SPDT switch: when closed, NO is LOW, NC is HIGH; when open, NO is HIGH, NC is LOW
    if (no == LOW) {
        return WindowState::CLOSED;
    } else if (nc == LOW) {
        return WindowState::OPEN;
    } else {
        // Both HIGH - should not happen with SPDT, default to CLOSED or handle error
        DEBUG_PRINTLN("Warning: Both NO and NC are HIGH - possible wiring issue");
        return WindowState::CLOSED;
    }
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
