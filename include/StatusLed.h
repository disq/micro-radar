#pragma once

#include <Arduino.h>

// Pico Display 2.0 RGB LED on GP6/7/8. It's common-anode, so a pin driven LOW
// lights that channel and HIGH turns it off. Kept off except for brief blinks.
namespace StatusLed {
    constexpr int PIN_R = 6, PIN_G = 7, PIN_B = 8;
    constexpr int ON = LOW, OFF = HIGH; // common-anode; swap these if your LED is inverted

    inline void Off() {
        digitalWrite(PIN_R, OFF);
        digitalWrite(PIN_G, OFF);
        digitalWrite(PIN_B, OFF);
    }

    inline void Begin() {
        pinMode(PIN_R, OUTPUT);
        pinMode(PIN_G, OUTPUT);
        pinMode(PIN_B, OUTPUT);
        Off();
    }

    inline void Blink(bool r, bool g, bool b, int ms = 60) {
        digitalWrite(PIN_R, r ? ON : OFF);
        digitalWrite(PIN_G, g ? ON : OFF);
        digitalWrite(PIN_B, b ? ON : OFF);
        delay(ms);
        Off();
    }

    inline void Blue()  { Blink(false, false, true); }
    inline void Red()   { Blink(true,  false, false); }
    inline void Green() { Blink(false, true,  false); }
}
