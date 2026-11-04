// The custom car file (.m2car, runtime/car_mod.h): a small car loads, with its
// extent; files without four wheels, a body, or with bad numbers do not.
#include "runtime/car_mod.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {

std::string write(const char *name, const std::string &text) {
    const std::string path = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/" + name;
    std::ofstream(path) << text;
    return path;
}

} // namespace

int main() {
    int failures = 0;
    auto check = [&](bool value, const char *message) {
        if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
    };
    const std::string wheels = "wheel FL -0.6 0.27 1.0 0.3\nwheel FR 0.6 0.27 1.0 0.3\n"
                               "wheel RL -0.6 0.27 -1.0 0.3\nwheel RR 0.6 0.27 -1.0 0.3\n";
    const std::string tri = "0 0 0  1 0 0  0 1 2  0 0 1  31\n";
    std::string error;
    auto car = rt::CarMod::load(write("car_ok.m2car", "m2car 1\n# a comment\n" + wheels + "body 1\n" + tri +
                                                          "wheelmesh FL 1\n" + tri),
                                &error);
    check(car && car->body.size() == 1 && car->wheel_meshes[rt::CarMod::FL].size() == 1, "a small car loads");
    check(car && car->body[0].color == 31 && car->wheels[rt::CarMod::RR][2] == -1.0f, "its colour and wheels");
    check(car && car->lo[0] == 0 && car->hi[0] == 1 && car->hi[2] == 2, "its extent");
    auto crashed = rt::CarMod::load(write("car_damaged.m2car", "m2car 1\n" + wheels + "body 1\n" + tri + "damaged 2\n" + tri +
                                                                   "0 0 0  1 0 0  0 1 2  0 0 1  " + std::to_string(31 + 0x60000) + "\n"),
                                    &error);
    check(crashed && crashed->damaged.size() == 2 && crashed->body.size() == 1, "a crashed model loads");
    check(crashed && !crashed->damaged[0].hood && crashed->damaged[1].hood && crashed->damaged[1].no_bonnet &&
              crashed->damaged[1].color == 31, "its hood and hidden parts");
    check(!rt::CarMod::load(write("car_wheels.m2car", "m2car 1\nwheel FL 0 0 0 0.3\nbody 1\n" + tri), &error),
          "four wheels are needed");
    check(!rt::CarMod::load(write("car_body.m2car", "m2car 1\n" + wheels), &error), "a body is needed");
    check(!rt::CarMod::load(write("car_bad.m2car", "m2car 1\n" + wheels + "body 1\n0 0 nan 1 0 0 0 1 0 0 0 1 3\n"),
                            &error),
          "bad numbers are refused");
    check(!rt::CarMod::load(write("car_magic.m2car", "obj 1\n"), &error) && !error.empty(), "not a .m2car");
    rt::set_car_mod(car);
    check(rt::car_mod() == car, "the player's car is set");
    rt::set_car_mod(nullptr);
    check(!rt::car_mod(), "and cleared");
    return failures ? 1 : 0;
}
