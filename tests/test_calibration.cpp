// The monitor calibration's table: MAME's video_start table by default, bit
// for bit (the lockstep and frame checks depend on it), and the settings.
#include "runtime/calibration.h"

#include <algorithm>
#include <cstdio>

int main() {
    int failures = 0;
    auto check = [&](bool value, const char *message) {
        if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
    };
    uint8_t table[256], mame[256];
    rt::fill_gamma_table(table);
    for (int i = 0; i < 256; i++) mame[i] = uint8_t(std::max((double(i) - 64.0) * 255.0 / 191.0, 0.0));
    check(std::equal(table, table + 256, mame), "the default is MAME's table");
    check(rt::calibration() == rt::Calibration{}, "the process starts with the default");

    const uint32_t generation = rt::calibration_generation();
    rt::set_calibration({88, 248, 1.2f});
    check(rt::calibration_generation() == generation + 1, "a change bumps the generation");
    rt::set_calibration({88, 248, 1.2f});
    check(rt::calibration_generation() == generation + 1, "the same setting does not");
    rt::fill_gamma_table(table, rt::calibration());
    check(table[88] == 0 && table[248] == 255 && table[255] == 255, "black and white points");
    check(table[140] < mame[140], "deeper blacks and mid tones than MAME's");
    bool monotonic = true;
    for (int i = 1; i < 256; i++) monotonic &= table[i] >= table[i - 1];
    check(monotonic, "the table never decreases");

    rt::set_calibration({250, 10, 99.0f});
    check(rt::calibration().black == 192 && rt::calibration().white == 224 && rt::calibration().gamma == 2.0f,
          "settings are clamped");
    return failures ? 1 : 0;
}
