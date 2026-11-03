// Launcher settings, saved to launcher.ini in the user's data folder (SDL's
// pref path): the ROM set, the GPU backend, fullscreen, audio volume, and
// the control bindings. Plain key=value lines, so it can be edited by hand.
#pragma once

#include "app/controls.h"

#include <string>
#include <cstdint>
#include <iosfwd>

namespace app {

struct Config {
    std::string rom_path;
    std::string gpu;          // "" (automatic), vulkan, direct3d12, metal
    std::string renderer = "software"; // the 3D: software (CPU, exact) or hardware (SDL_GPU)
    bool fullscreen = false;
    bool hide_cursor = false; // no mouse cursor while the game plays; it shows again in the launcher
    // Exclusive fullscreen in this mode ("WxH@Hz", with "*density" when not
    // 1), or "" for borderless at the desktop's mode.
    std::string fullscreen_mode;
    // Frame pacing (#7, app/pacing.h); all off: the arcade's speed on any display.
    bool pace_smooth = false;       // a display at a multiple of 57.52 Hz: one frame per refresh(es)
    bool pace_sync_display = false; // the game at a rate dividing the refresh (60 on 60/120/240 Hz: 4% fast)
    bool pace_vrr = false;          // each frame held to 1/57.52 s, for a variable-refresh display
    bool skip_launcher = false; // start the game straight away (as --autostart); Esc still opens the launcher
    // Hold the cabinet's Test button for 3 s once the game runs (rt::TestHold),
    // for the test menu without an F2 key: mobile, consoles. Not saved; it
    // clears itself when done.
    bool hold_test = false;
    float volume = 0.8f;      // 0..1
    bool mute = false;
    float music_volume = 1.0f;   // 0..1: the music against the effects (both 1: as the arcade mixes them)
    float effects_volume = 1.0f; // 0..1
    bool native_audio = false; // applies on reset; reference remains the default
    // Enhancements (off by default).
    std::string aspect;        // widescreen: "" (original), "16:10", "16:9", "21:9"
    bool hud_edges = false;    // with widescreen: lap times, position and maps at the screen edges
    bool stretch_backdrop = false; // with widescreen, in-game: the tile backdrop stretched across the width, else plain sky
    int draw_distance = 0;     // scenery: 0 = the game's own, -2..+2 (rt::Enhance)
    uint32_t draw_budget = 0;   // 0 Automatic; positive Custom allowance (runtime/scenery.h)
    int draw_mode = 0;         // 0 double buffered (every frame), 1 single buffered (every 2nd), 2 every third frame
    int supersampling = 1;     // hardware renderer: drawn at 1 (off) to kMaxSupersampling times the original resolution; 0 matches the window
    static constexpr int kMaxSupersampling = 8;
    int texture_detail = 0;    // hardware renderer: 0 the game's own mip levels, 1-2 that many levels finer
    // The frame scaled to the window: "" smooth (bilinear), "sharp" (nearest),
    // "integer" (nearest, whole multiples of the original pixels only).
    std::string scaling;
    // CRT filter (app/gpu/crt.h): 0 off; 1 slot mask (arcade monitor), 2 aperture
    // grille, 3 shadow mask, 4 scanlines only. Its settings 0..1 (curvature 0..0.25).
    int crt = 0;
    float crt_scanlines = 0.9f, crt_mask = 0.35f, crt_glow = 0.12f, crt_curvature = 0.04f;
    // Custom player car (runtime/car_mod.h; Revision A): a .m2car made by
    // scripts/make_car_mod.py, with its own wheels or the game's under it.
    bool car_mod = false;
    std::string car_path;
    bool car_own_wheels = true;
    // Monitor calibration (runtime/calibration.h): the colour DAC's black and
    // white points and a gamma. 64, 255, 1: MAME's conversion, the default.
    int cal_black = 64, cal_white = 255;
    float cal_gamma = 1.0f;
    // The "Arcade monitor" preset: deeper blacks, a little more contrast and a
    // CRT-like steeper mid tone response.
    static constexpr int kArcadeBlack = 88, kArcadeWhite = 248;
    static constexpr float kArcadeGamma = 1.2f;
    static constexpr double kMaxAspect = 21.0 / 9.0;
    double aspect_ratio() const; // width / height; 0 = original
    Controls controls;
    // Link play (the communication board; Revision A): this cabinet listens
    // for the one before it in the ring and connects to the next. Master or
    // slave, and the car number, are the game's own settings (test mode).
    bool link = false;
    int link_port = 15112;               // where the cabinet before this one connects
    std::string link_next = "127.0.0.1:15113"; // host:port of the next cabinet
    bool link_framesync = false;         // hold every cabinet to the master's frame
    float ffb_strength = 0.7f; // force feedback (the drive board) on the steering device: 0 off .. 1
    bool ffb_invert = false;   // turn the wheel the other way
    bool ffb_log = false;      // log the force feedback device, failures and the commands (daytona.log on Windows)
    bool ffb_centring_override = false; // Linux: the game's centring worked out here, sent as constant force (#16)
    // Linux and macOS: Logitech wheels through the system's driver, not SDL's
    // own (its HIDAPI lg4ff driver, off on Windows already). Some, like the
    // original Driving Force, send SDL's driver nothing (issue #4). On by
    // default on Linux, whose kernel driver gives the wheels force feedback;
    // off on macOS, where only SDL's driver does. Applies at start-up; SDL
    // gives the wheel another GUID, so it is bound again.
#ifdef __linux__
    bool legacy_logitech_wheels = true;
#else
    bool legacy_logitech_wheels = false;
#endif

    Config() { controls.set_defaults(); }
    static std::string path();  // <pref path>/launcher.ini
    // --profile NAME: a separate data folder (settings, EEPROM, backup RAM),
    // e.g. a second cabinet on the same computer for link play.
    static inline std::string profile;
    static std::string pref_dir(); // where this set's (and profile's) data lives, with a trailing separator
    void load();
    void save() const;
    void read(std::istream &input);
    void write(std::ostream &output) const;
};

} // namespace app
