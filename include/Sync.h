#pragma once

#include "pico/mutex.h"

// Cross-core coordination. The OpenSky fetch (slow, blocking TLS) runs on core0
// while the radar is drawn on core1, so the animation never stalls during a poll.
extern mutex_t g_dataMutex;        // guards AircraftManager's tracked aircraft (core0 merge vs core1 draw)
extern mutex_t g_displayMutex;     // guards the physical TFT/SPI (core0 status text vs core1 sprite push)
extern volatile bool g_radarActive; // core1 draws the radar only once core0 has booted + connected
