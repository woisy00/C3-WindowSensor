#pragma once

#include <strings.h>

enum class ReedMode : unsigned char { SPDT = 0, SINGLE = 1 };
enum class OpenLevel : unsigned char { High = 0, Low = 1 };

namespace ReedModeUtil {
    inline ReedMode fromStored(unsigned long v) {
        return v == 1 ? ReedMode::SINGLE : ReedMode::SPDT;
    }
    inline OpenLevel openLevelFromStored(unsigned long v) {
        return v == 1 ? OpenLevel::Low : OpenLevel::High;
    }
    inline bool parseMode(const char* text, ReedMode& result) {
        if (text && strcasecmp(text, "spdt") == 0) { result = ReedMode::SPDT; return true; }
        if (text && strcasecmp(text, "single") == 0) { result = ReedMode::SINGLE; return true; }
        return false;
    }
    inline bool parseOpenLevel(const char* text, OpenLevel& result) {
        if (text && strcasecmp(text, "high") == 0) { result = OpenLevel::High; return true; }
        if (text && strcasecmp(text, "low") == 0) { result = OpenLevel::Low; return true; }
        return false;
    }
    inline const char* toText(ReedMode mode) { return mode == ReedMode::SINGLE ? "single" : "spdt"; }
    inline const char* toText(OpenLevel level) { return level == OpenLevel::Low ? "LOW" : "HIGH"; }
}

namespace ReedLogic {
    struct State {
        int no = 0; // raw GPIO2 (unused in single mode)
        int nc = 0; // raw GPIO1 in both modes
        ReedMode mode = ReedMode::SPDT;
        OpenLevel openLevel = OpenLevel::High;

        bool valid() const { return mode == ReedMode::SINGLE || no != nc; }
        bool isOpen() const {
            if (mode == ReedMode::SINGLE) return (nc != 0) == (openLevel == OpenLevel::High);
            return no != 0 || nc == 0; // preserve legacy SPDT reporting for invalid states
        }
        bool matches(const State& other) const {
            return mode == other.mode && openLevel == other.openLevel && nc == other.nc &&
                   (mode == ReedMode::SINGLE || no == other.no);
        }
    };

    struct WakePlan {
        bool reedEnabled;
        bool reedHigh;
        bool buttonHigh;
        bool retry;
    };
    inline WakePlan wakePlan(const State& state) {
        return {state.valid(), state.nc == 0, false, !state.valid()};
    }
}
