#include "Hardware.h"
#include "Utils.h"
#include <Arduino.h>
#include "esp_adc_cal.h"
#include "WindowSensorDefines.h"

esp_adc_cal_characteristics_t adc_chars;

void Hardware::initPins() {
    // reed switch inputs
    pinMode(PIN_NO, INPUT);
    pinMode(PIN_NC, INPUT);
    // battery ADC pin doesn't need mode for analogRead, but
    // setting it explicitly avoids accidental digital writes
    pinMode(PIN_BAT_ADC, INPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    // battery enable pin
    pinMode(PIN_BAT_EN, OUTPUT);
    digitalWrite(PIN_BAT_EN, HIGH);
}

bool Hardware::readWindowState() {
    delay(10); // allow settling
    bool no = digitalRead(PIN_NO);
    //bool nc = digitalRead(PIN_NC);
    DEBUG_PRINTLN("Read window state: " + Utils::boolToString(no));
    // read the NO and NC pins and determine window state
    return no == LOW; // || nc == HIGH;
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


    // enable battery reading
    digitalWrite(PIN_BAT_EN, LOW);
    delay(50); // allow settling

    // discard first sample
    analogRead(PIN_BAT_ADC);
    delay(10);

    uint32_t sum = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        sum += analogRead(PIN_BAT_ADC);
        delay(2);
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
