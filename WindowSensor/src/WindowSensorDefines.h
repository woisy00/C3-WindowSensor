// #define VERSION "v1.0.199"

// #define DEBUG
// #define DEBUG_REMOTE

#if defined(DEBUG)
    #include <Arduino.h>

    #define DEBUG_INIT() Serial.begin(115200);
    #define DEBUG_PRINTLN(x) Serial.println(x);
#else
    #define DEBUG_INIT()
    #define DEBUG_PRINTLN(x)
#endif


#define HW_MODEL "BWS-01"
#define HW_MANUFACTURER "bergermeier"