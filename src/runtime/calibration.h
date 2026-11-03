// Monitor calibration: how the colour translation's output (the video DAC's
// 0-255) becomes the picture's 0-255. MAME's model2 video_start maps
// (i - 64) * 255 / 191, clamped: an approximation it says works for most
// games, real cabinets having had their monitors set up per game. That is the
// default, exactly. black: the input shown as black (64); white: the input
// shown as full white (255); gamma: an exponent on the result (1: none; more
// darkens the mid tones, as a CRT's steeper response).
//
// One setting for the process (the launcher's); Video, Raster and the
// hardware renderer refill their tables when its generation changes.
#pragma once

#include <cstdint>

namespace rt {

struct Calibration {
    int black = 64;
    int white = 255;
    float gamma = 1.0f;
    bool operator==(const Calibration &) const = default;
};

// The table for c; with the default, MAME's video_start table.
void fill_gamma_table(uint8_t table[256], const Calibration &c = {});

const Calibration &calibration();
void set_calibration(const Calibration &c); // clamps; a different setting bumps the generation
uint32_t calibration_generation();

} // namespace rt
