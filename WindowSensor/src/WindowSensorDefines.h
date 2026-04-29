// #define DEBUG
// #define ARDUINOHA_DEBUG


#if defined(DEBUG)
    #include <Arduino.h>

    #define DEBUG_INIT() Serial.begin(115200);
    #define DEBUG_PRINTLN(x) Serial.println(x);
    #define DEBUG_PRINT(x) Serial.print(x);
#else
    #define DEBUG_INIT()
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINT(x)
#endif