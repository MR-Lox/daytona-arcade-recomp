#include "runtime/calibration.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace rt {

namespace {

Calibration g_calibration;
std::atomic<uint32_t> g_generation{0};

} // namespace

void fill_gamma_table(uint8_t table[256], const Calibration &c) {
    const double range = double(c.white - c.black);
    for (int i = 0; i < 256; i++) {
        // MAME video_start: max((i - 64) * 255 / 191, 0), truncated
        double raw_value = std::max((double(i) - double(c.black)) * 255.0 / range, 0.0);
        if (c.gamma != 1.0f) raw_value = 255.0 * std::pow(std::min(raw_value / 255.0, 1.0), double(c.gamma));
        table[i] = uint8_t(std::min(raw_value, 255.0));
    }
}

const Calibration &calibration() { return g_calibration; }

void set_calibration(const Calibration &c) {
    Calibration v = c;
    v.black = std::clamp(v.black, 0, 192);
    v.white = std::clamp(v.white, v.black + 32, 255);
    v.gamma = std::isfinite(v.gamma) ? std::clamp(v.gamma, 0.5f, 2.0f) : 1.0f;
    if (v == g_calibration) return;
    g_calibration = v;
    g_generation.fetch_add(1);
}

uint32_t calibration_generation() { return g_generation.load(); }

} // namespace rt
