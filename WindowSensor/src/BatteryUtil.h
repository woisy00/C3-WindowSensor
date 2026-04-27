#pragma once

enum BatteryType {
    ALKALINE,
    NIMH
};

namespace BatteryUtil {
    struct Point {
        float v;
        float p;
    };

    // Convert battery voltage to percentage based on battery type
    float voltageToPercent(float v, BatteryType type);
}
