#pragma once

class SleepManager {
public:
    static void setupWake(bool windowOpen, unsigned long sleepTime);
    static void goToSleep(bool windowOpen, unsigned long sleepTime);
};
