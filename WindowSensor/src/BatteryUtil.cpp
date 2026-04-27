#include "BatteryUtil.h"

// --- Battery curves ---

static const BatteryUtil::Point alkalineCurve[] = {
    {3.2, 100},
    {3.1, 95},
    {3.0, 90},
    {2.9, 80},
    {2.8, 65},
    {2.7, 50},
    {2.6, 35},
    {2.5, 20},
    {2.4, 10},
    {2.3, 5},
    {2.1, 2},
    {2.0, 0}
};

static const BatteryUtil::Point nimhCurve[] = {
    {2.8, 100},
    {2.7, 95},
    {2.6, 90},
    {2.5, 80},
    {2.4, 60},
    {2.35, 40},
    {2.3, 20},
    {2.2, 10},
    {2.1, 5},
    {2.0, 0}
};

// --- Battery logic ---

static float interpolate(const BatteryUtil::Point* curve, int size, float v) {
    if (v >= curve[0].v) return 100;
    if (v <= curve[size - 1].v) return 0;

    for (int i = 0; i < size - 1; i++) {
        if (v <= curve[i].v && v > curve[i + 1].v) {
            float v1 = curve[i].v;
            float v2 = curve[i + 1].v;
            float p1 = curve[i].p;
            float p2 = curve[i + 1].p;

            return p2 + (v - v2) * (p1 - p2) / (v1 - v2);
        }
    }

    return 0;
}

float BatteryUtil::voltageToPercent(float v, BatteryType type) {
    // compensate WiFi voltage drop (~50mV typical)
    v += 0.05f;

    if (type == BatteryType::NIMH) {
        return interpolate(
            nimhCurve,
            sizeof(nimhCurve) / sizeof(Point),
            v
        );
    } else {
        return interpolate(
            alkalineCurve,
            sizeof(alkalineCurve) / sizeof(Point),
            v
        );
    }
}
