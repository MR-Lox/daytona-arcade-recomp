// Persisted scenery settings round-trip through the same format as launcher.ini.
// Streams keep tests isolated from the user's live profile.
#include "app/config.h"
#include <cstdio>
#include <sstream>

int main() {
    int failures = 0;
    auto check = [&](bool value, const char *message) {
        if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
    };
    app::Config original;
    check(original.draw_budget == 0 && original.aspect_ratio() == 0, "new installs preserve original defaults");
    original.aspect = "21:9";
    original.draw_distance = 2;
    original.draw_budget = 5000;
    std::ostringstream output;
    original.write(output);
    app::Config loaded;
    std::istringstream input(output.str());
    loaded.read(input);
    check(loaded.aspect_ratio() == 21.0 / 9.0 && loaded.draw_budget == 5000 && loaded.draw_distance == 2,
          "custom budget independent of aspect and distance survives save/load");
    std::istringstream automatic("draw_budget=0\naspect=21:9\n");
    loaded.read(automatic);
    check(loaded.draw_budget == 0 && loaded.aspect_ratio() == 21.0 / 9.0, "return to Automatic survives load");
    loaded.aspect = "32:9";
    check(loaded.aspect_ratio() == 21.0 / 9.0, "saved future aspect is capped at the supported view");
    for (auto value : {"-1", "1000001", "4294967296", "123oops", ""}) {
        std::istringstream bad(std::string("draw_budget=") + value + "\n");
        loaded.draw_budget = 9000;
        loaded.read(bad);
        check(loaded.draw_budget == 0, "invalid budget falls back to Automatic");
    }
    std::istringstream legacy("aspect=16:9\ndraw_distance=1\n");
    app::Config old;
    old.read(legacy);
    check(old.draw_budget == 0 && old.draw_distance == 1, "old settings use Automatic");
    for (auto value : {"nan:9", "21:inf", "21:0", "-21:9"}) {
        loaded.aspect = value;
        check(loaded.aspect_ratio() == 0, "non-finite and invalid aspect rejected");
    }
    check(old.supersampling == 1 && old.texture_detail == 0 && old.scaling.empty(), "graphics default to the original");
    app::Config graphics;
    graphics.supersampling = 0;
    graphics.texture_detail = 2;
    graphics.scaling = "integer";
    std::ostringstream graphics_out;
    graphics.write(graphics_out);
    app::Config graphics_in;
    std::istringstream graphics_stream(graphics_out.str());
    graphics_in.read(graphics_stream);
    check(graphics_in.supersampling == 0 && graphics_in.texture_detail == 2 && graphics_in.scaling == "integer",
          "match-window super sampling, texture detail and scaling survive save/load");
    std::istringstream graphics_bad("supersampling=99\ntexture_detail=-3\nscaling=blurry\n");
    graphics_in.read(graphics_bad);
    check(graphics_in.supersampling == app::Config::kMaxSupersampling && graphics_in.texture_detail == 0 &&
              graphics_in.scaling.empty(),
          "out of range graphics settings are clamped");
    check(old.crt == 0, "the CRT filter is off by default");
    app::Config crt;
    crt.crt = 3;
    crt.crt_scanlines = 0.5f;
    crt.crt_curvature = 0.1f;
    std::ostringstream crt_out;
    crt.write(crt_out);
    app::Config crt_in;
    std::istringstream crt_stream(crt_out.str());
    crt_in.read(crt_stream);
    check(crt_in.crt == 3 && crt_in.crt_scanlines == 0.5f && crt_in.crt_curvature == 0.1f, "CRT settings survive save/load");
    std::istringstream crt_bad("crt=9\ncrt_glow=nan\ncrt_mask=7\ncrt_curvature=-1\n");
    crt_in.read(crt_bad);
    check(crt_in.crt == 4 && crt_in.crt_glow == app::Config().crt_glow && crt_in.crt_mask == 1.0f &&
              crt_in.crt_curvature == 0.0f,
          "out of range CRT settings are clamped, invalid ones ignored");
    return failures ? 1 : 0;
}
