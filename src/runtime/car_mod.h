// Custom player car (enhancement, off by default): a model of your own drawn
// in place of the player's car body, from a .m2car file that
// scripts/make_car_mod.py writes from an OBJ. The geometrizer (Geo) draws its
// polygons with the body's matrix, lit as the game lights its own, each with
// its own colour (GeoPoly::direct_color) where the game's take a palette
// entry. The game's wheels are moved under it (and scaled to its wheels), or
// its own wheels are drawn with the game wheels' matrices, so they still turn
// and steer. Crashes dent it as they dent the game's body (Geo::dented_car_body:
// each point moves as the game's dented model's points around the same place).
// The game itself (physics, collisions) is unchanged.
//
// .m2car, text: the car's body space is metres, x right, y up, z ahead, the
// ground at y 0, as the game's car (tools/m2modeldump).
//   m2car 1
//   wheel FL|FR|RL|RR  x y z radius        the wheel centres
//   wheelwidth W                           (optional) the tyres' width: the game's are drawn that wide
//   body N                                 then N triangles:
//   x y z  x y z  x y z  nx ny nz  rgb555  corners, outward normal, colour
//                                          (+ 0x8000: glass, drawn reflecting the sky;
//                                           + 0x10000: a decal, drawn over what it lies on;
//                                           + 0x20000: not drawn in the bonnet view (wipers);
//                                           + 0x40000: the hood, opened by a hit at the front)
//   wheelmesh FL|FR|RL|RR N                (optional) that wheel's triangles, body space
//   damaged N                              (optional) triangles of the crashed car, shown once both
//                                          rear corners are dented (the front's dents added)
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rt {

struct CarMod {
    struct Tri {
        float p[3][3];
        float n[3];
        uint16_t color; // RGB555
        bool glass;     // a window: its colour the sky it reflects
        bool decal;     // sorted a little nearer: over the faces it lies on (painter's order)
        bool no_bonnet; // not drawn in the bonnet view
        bool hood;      // turns on its hinge (its rear edge) once the front is dented
    };
    enum Corner { FL, FR, RL, RR };
    std::array<std::array<float, 4>, 4> wheels{}; // x y z radius, by Corner
    float wheel_width = 0;                        // 0: the game's tyres scaled with their radius
    std::vector<Tri> body;
    std::vector<Tri> damaged; // empty: none in the file
    std::array<std::vector<Tri>, 4> wheel_meshes; // empty: none in the file
    bool own_wheels = false; // draw wheel_meshes, else the game's wheels moved and scaled
    float lo[3] = {}, hi[3] = {}; // the body's extent (its shadow is scaled to it)

    // Reads a .m2car; false with *error set when it cannot.
    static std::shared_ptr<CarMod> load(const std::string &path, std::string *error);
};

// The car the geometrizer draws for the player (null: the game's own).
const std::shared_ptr<const CarMod> &car_mod();
void set_car_mod(std::shared_ptr<const CarMod> mod);

} // namespace rt
