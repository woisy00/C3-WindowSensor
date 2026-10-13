#pragma once

#include <stdint.h>

// Tracks one continuous button hold. Pure logic (time and level are injected) so it can be unit tested.
// A release shorter than RELEASE_DEBOUNCE_MS is treated as contact bounce and does not restart the hold.
class ButtonHold {
public:
    static constexpr uint32_t HOLD_MS = 3000;
    static constexpr uint32_t RELEASE_DEBOUNCE_MS = 30;

    void update(bool pressed, uint32_t nowMs) {
        if (pressed) {
            if (!_pressed) {
                _pressed = true;
                _start = nowMs;
            }
            _releasing = false;
            if (!_fired && (uint32_t)(nowMs - _start) >= HOLD_MS) _fired = true;
        } else if (_pressed) {
            if (!_releasing) {
                _releasing = true;
                _releaseStart = nowMs;
            } else if ((uint32_t)(nowMs - _releaseStart) >= RELEASE_DEBOUNCE_MS) {
                _pressed = false;
                _releasing = false;
            }
        }
    }

    // hold reached HOLD_MS (latched until reset())
    bool fired() const { return _fired; }
    // a hold is in progress and has not yet been decided
    bool pending() const { return _pressed && !_fired; }
    void reset() { *this = ButtonHold(); }

private:
    bool _pressed = false;
    bool _releasing = false;
    bool _fired = false;
    uint32_t _start = 0;
    uint32_t _releaseStart = 0;
};
