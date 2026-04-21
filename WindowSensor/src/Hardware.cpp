#include "Hardware.h"
#include "Utils.h"
#include <Arduino.h>

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
    bool no = digitalRead(PIN_NO);
#ifdef DEBUG
    Utils::logMsg("Read window state: " + Utils::boolToString(no));
#endif
    return no == LOW;
}

float Hardware::readBattery(float rTop, float rBottom) {
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);

    // enable battery reading
    digitalWrite(PIN_BAT_EN, LOW);
    delay(10); // allow settling

    // discard first sample
    analogRead(PIN_BAT_ADC);
    delay(10);

    uint32_t sum = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        sum += analogRead(PIN_BAT_ADC);
        delay(2);
    }

    uint16_t raw = sum / BATTERY_SAMPLES;

    float adc_voltage = (raw / 4095.0) * 2.5;

    if (rBottom <= 0.0) {
#ifdef DEBUG
        Utils::logMsg("Warning: rBottom is zero or negative, defaulting to 1 kΩ");
#endif
        rBottom = 1.0;
    }
    float ratio = (rTop + rBottom) / rBottom;
    float battery_voltage = adc_voltage * ratio;

#ifdef DEBUG
    Utils::logMsg("Battery state: " + String(battery_voltage) + " (rTop=" + String(rTop) + "kΩ, rBottom=" + String(rBottom) + "kΩ)");
#endif
    return battery_voltage;
}

bool Hardware::bootButtonPressed() {
    delay(20);
    return digitalRead(PIN_BUTTON) == LOW;
}
