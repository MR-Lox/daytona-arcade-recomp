#include "runtime/car_mod.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace rt {

namespace {

std::shared_ptr<const CarMod> g_car_mod;

int corner(const std::string &name) {
    static const char *names[] = {"FL", "FR", "RL", "RR"};
    for (int i = 0; i < 4; i++)
        if (name == names[i]) return i;
    return -1;
}

bool read_tris(std::istream &in, size_t count, std::vector<CarMod::Tri> &out) {
    out.reserve(count);
    for (size_t i = 0; i < count; i++) {
        CarMod::Tri t{};
        unsigned color = 0;
        for (auto &p : t.p)
            for (float &c : p) in >> c;
        for (float &c : t.n) in >> c;
        in >> color;
        if (!in) return false;
        for (const auto &p : t.p)
            for (float c : p)
                if (!std::isfinite(c)) return false;
        t.color = uint16_t(color & 0x7fff);
        out.push_back(t);
    }
    return true;
}

} // namespace

std::shared_ptr<CarMod> CarMod::load(const std::string &path, std::string *error) {
    auto fail = [&](const std::string &why) -> std::shared_ptr<CarMod> {
        if (error) *error = why;
        return nullptr;
    };
    std::ifstream in(path);
    if (!in) return fail("cannot open " + path);
    std::string magic;
    int version = 0;
    in >> magic >> version;
    if (magic != "m2car" || version != 1) return fail("not a .m2car file (version 1)");
    auto mod = std::make_shared<CarMod>();
    int wheels = 0;
    std::string word;
    while (in >> word) {
        if (word == "wheel") {
            std::string name;
            in >> name;
            const int c = corner(name);
            if (c < 0) return fail("unknown wheel " + name);
            for (float &v : mod->wheels[c]) in >> v;
            if (!in || !(mod->wheels[c][3] > 0)) return fail("bad wheel " + name);
            wheels |= 1 << c;
        } else if (word == "body" || word == "wheelmesh") {
            int c = -1;
            if (word == "wheelmesh") {
                std::string name;
                in >> name;
                if ((c = corner(name)) < 0) return fail("unknown wheel " + name);
            }
            size_t count = 0;
            in >> count;
            if (!in || count > 200000) return fail("bad " + word + " count");
            if (!read_tris(in, count, c < 0 ? mod->body : mod->wheel_meshes[c])) return fail("bad " + word + " data");
        } else if (!word.empty() && word[0] == '#') {
            std::getline(in, word);
        } else {
            return fail("unexpected '" + word + "'");
        }
    }
    if (wheels != 15) return fail("the four wheels (FL FR RL RR) are needed");
    if (mod->body.empty()) return fail("no body");
    for (int k = 0; k < 3; k++) mod->lo[k] = mod->hi[k] = mod->body[0].p[0][k];
    for (const Tri &t : mod->body)
        for (const auto &p : t.p)
            for (int k = 0; k < 3; k++) mod->lo[k] = std::min(mod->lo[k], p[k]), mod->hi[k] = std::max(mod->hi[k], p[k]);
    return mod;
}

const std::shared_ptr<const CarMod> &car_mod() { return g_car_mod; }

void set_car_mod(std::shared_ptr<const CarMod> mod) { g_car_mod = std::move(mod); }

} // namespace rt
