#pragma once

// Pico Display 2.0 is 320x240. The radar is a circle centred on the panel,
// sized to the shorter (vertical) axis, leaving the side margins as bezel.
constexpr int SCREEN_WIDTH = 320;
constexpr int SCREEN_HEIGHT = 240;

constexpr int RADAR_CENTRE_X = SCREEN_WIDTH / 2;   // 160
constexpr int RADAR_CENTRE_Y = SCREEN_HEIGHT / 2;  // 120
constexpr int RADAR_RADIUS = SCREEN_HEIGHT / 2;    // 120
