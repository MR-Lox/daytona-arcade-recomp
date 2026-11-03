// m2modeldump: a 3D model the game draws, as an OBJ file (with its colours),
// from the geometrizer's model capture (Geo::Capture). For personal use with
// your own ROM set: the output is derived from the game's data, so it stays
// in git-ignored folders and is never shared (rules.md).
//
//   m2modeldump IMAGES_DIR --frame N [--inputs FILE] [--nvram DIR] --list [--near Z]
//   m2modeldump IMAGES_DIR --frame N [--inputs FILE] [--nvram DIR] --objects A[,B...] --out DIR
//               [--frames K]
//   m2modeldump IMAGES_DIR --frame N [--inputs FILE] [--nvram DIR] --drawn DIR [--near Z]
//               [--car FILE.m2car [--own-wheels]]
//
// --list: the objects drawn at frame N (polygon address, polygon count, the
// object's position in view space, window), nearest first; --near Z keeps
// those closer than Z.
// --objects: the objects to export (addresses from --list, hex), the first
// the reference: every polygon is brought into its model space, so parts
// with their own matrices (wheels) stay in place. --frames K: the same
// objects over K frames from N, their polygons merged (faces one frame did
// not draw), duplicates dropped. Writes DIR/model.obj and DIR/model.mtl
// (a colour per material: the polygon's palette colour).
// --drawn: what the geometrizer kept at frame N (after culling and clipping,
// a custom car included: --car), the polygons nearer than Z (8 by default),
// in view space, as DIR/drawn.obj: to look at a custom car from any side.
#include "runtime/car_mod.h"
#include "runtime/game_loop.h"
#include "runtime/geo.h"
#include "../common/input_script.h"
#include "../common/nvram.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace {

using Capture = rt::Geo::Capture;

// view = R * model + T, then x and y times the focus (Geo transform_point,
// apply_focus); back to model space.
struct Frame {
    double r[9], t[3], inv[9], focus[2];
    explicit Frame(const rt::Geo::CaptureObject &o) {
        for (int i = 0; i < 9; i++) r[i] = o.matrix[i];
        for (int i = 0; i < 3; i++) t[i] = o.matrix[9 + i];
        focus[0] = o.focus[0], focus[1] = o.focus[1];
        // view_j = sum_i model_i * m[3i + j]: the matrix M[j][i] = m[3i + j]; invert M
        const auto M = [&](int j, int i) { return r[3 * i + j]; };
        const double det = M(0, 0) * (M(1, 1) * M(2, 2) - M(1, 2) * M(2, 1)) -
                           M(0, 1) * (M(1, 0) * M(2, 2) - M(1, 2) * M(2, 0)) +
                           M(0, 2) * (M(1, 0) * M(2, 1) - M(1, 1) * M(2, 0));
        double a[3][3];
        a[0][0] = (M(1, 1) * M(2, 2) - M(1, 2) * M(2, 1)) / det;
        a[0][1] = (M(0, 2) * M(2, 1) - M(0, 1) * M(2, 2)) / det;
        a[0][2] = (M(0, 1) * M(1, 2) - M(0, 2) * M(1, 1)) / det;
        a[1][0] = (M(1, 2) * M(2, 0) - M(1, 0) * M(2, 2)) / det;
        a[1][1] = (M(0, 0) * M(2, 2) - M(0, 2) * M(2, 0)) / det;
        a[1][2] = (M(0, 2) * M(1, 0) - M(0, 0) * M(1, 2)) / det;
        a[2][0] = (M(1, 0) * M(2, 1) - M(1, 1) * M(2, 0)) / det;
        a[2][1] = (M(0, 1) * M(2, 0) - M(0, 0) * M(2, 1)) / det;
        a[2][2] = (M(0, 0) * M(1, 1) - M(0, 1) * M(1, 0)) / det;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) inv[3 * i + j] = a[i][j];
    }
    void to_model(const float view[3], double out[3]) const {
        const double v[3] = {view[0] / focus[0] - t[0], view[1] / focus[1] - t[1], view[2] - t[2]};
        for (int i = 0; i < 3; i++) out[i] = inv[3 * i] * v[0] + inv[3 * i + 1] * v[1] + inv[3 * i + 2] * v[2];
    }
};

void usage() {
    std::fprintf(stderr, "usage: m2modeldump IMAGES_DIR --frame N [--inputs FILE] [--nvram DIR]\n"
                         "         (--list [--near Z] | --objects A[,B...] --out DIR [--frames K])\n");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) return usage(), 2;
    const std::string dir = argv[1];
    std::string inputs_path, nvram_dir, out_dir;
    uint64_t frame = 0, frames = 1;
    bool list = false, own_wheels = false;
    std::string drawn_dir, car_path;
    double near_z = 1e30;
    std::vector<uint32_t> wanted;
    for (int i = 2; i < argc; i++) {
        const bool more = i + 1 < argc;
        if (!std::strcmp(argv[i], "--list")) list = true;
        else if (!std::strcmp(argv[i], "--own-wheels")) own_wheels = true;
        else if (more && !std::strcmp(argv[i], "--drawn")) drawn_dir = argv[++i];
        else if (more && !std::strcmp(argv[i], "--car")) car_path = argv[++i];
        else if (!more) return usage(), 2;
        else if (!std::strcmp(argv[i], "--frame")) frame = std::strtoull(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--frames")) frames = std::max<uint64_t>(1, std::strtoull(argv[++i], nullptr, 10));
        else if (!std::strcmp(argv[i], "--inputs")) inputs_path = argv[++i];
        else if (!std::strcmp(argv[i], "--nvram")) nvram_dir = argv[++i];
        else if (!std::strcmp(argv[i], "--out")) out_dir = argv[++i];
        else if (!std::strcmp(argv[i], "--near")) near_z = std::strtod(argv[++i], nullptr);
        else if (!std::strcmp(argv[i], "--objects")) {
            for (char *p = argv[++i]; *p;) {
                wanted.push_back(uint32_t(std::strtoul(p, &p, 16)));
                if (*p == ',') ++p;
            }
        } else return usage(), 2;
    }
    if (!frame || (!list && drawn_dir.empty() && (wanted.empty() || out_dir.empty()))) return usage(), 2;
    if (!car_path.empty()) {
        std::string error;
        auto car = rt::CarMod::load(car_path, &error);
        if (!car) {
            std::fprintf(stderr, "m2modeldump: %s\n", error.c_str());
            return 1;
        }
        car->own_wheels = own_wheels;
        rt::set_car_mod(car);
    }

    try {
        rt::GameLoop game(dir);
        if (!nvram_dir.empty()) tools::load_nvram(game, nvram_dir);
        tools::Script script;
        if (!inputs_path.empty()) script.load(inputs_path);
        while (game.board().frame() + 1 < frame) game.run_frame(script.at(game.board().frame()));

        // the polygons of the wanted objects, over the frames, in the reference's model space
        struct Face {
            int n;
            double p[4][3];
            uint16_t texheader[4];
            uint16_t uv[4][2];
            uint8_t luma;
        };
        std::vector<Face> faces;
        std::set<std::vector<long long>> seen; // duplicates: the same corners (to 1/1000 of a unit)
        rt::Geo::Capture capture;
        for (uint64_t f = 0; f < frames; f++) {
            capture = {};
            game.board().geo().capture = &capture;
            game.run_frame(script.at(game.board().frame()));
            game.board().geo().capture = nullptr;
            if (!drawn_dir.empty()) { // the kept polygons near the camera, focus undone
                const float fx = capture.objects.empty() ? 1.0f : capture.objects[0].focus[0];
                const float fy = capture.objects.empty() ? 1.0f : capture.objects[0].focus[1];
                const double limit = near_z < 1e29 ? near_z : 8.0;
                const uint8_t *palram = game.board().video().gpu_mem().palram;
                FILE *obj = std::fopen((drawn_dir + "/drawn.obj").c_str(), "w");
                FILE *mtl = std::fopen((drawn_dir + "/drawn.mtl").c_str(), "w");
                if (!obj || !mtl) return std::fprintf(stderr, "m2modeldump: cannot write in %s\n", drawn_dir.c_str()), 1;
                std::fprintf(obj, "mtllib drawn.mtl\n");
                std::set<uint32_t> mats;
                int nv = 0, kept = 0;
                for (const rt::GeoPoly &p : game.board().geo().polys) {
                    bool near = true;
                    for (int k = 0; k < p.num_vertices; k++) near &= p.v[k].p[0] < limit;
                    if (!near) continue;
                    const uint32_t colorbase = (p.texheader[3] >> 6) & 0x3ff;
                    const uint16_t c = p.direct_color ? uint16_t(p.direct_color & 0x7fff)
                                                      : uint16_t(palram[(colorbase + 0x1000) * 2] |
                                                                 palram[(colorbase + 0x1000) * 2 + 1] << 8);
                    if (mats.insert(c).second)
                        std::fprintf(mtl, "newmtl m%04x\nKd %.3f %.3f %.3f\n", c, (c & 31) / 31.0,
                                     (c >> 5 & 31) / 31.0, (c >> 10 & 31) / 31.0);
                    std::fprintf(obj, "usemtl m%04x\n", c);
                    for (int k = 0; k < p.num_vertices; k++)
                        std::fprintf(obj, "v %.4f %.4f %.4f\n", p.v[k].x / fx, p.v[k].y / fy, p.v[k].p[0]);
                    std::fprintf(obj, "f");
                    for (int k = 0; k < p.num_vertices; k++) std::fprintf(obj, " %d", nv + k + 1);
                    std::fprintf(obj, "\n");
                    nv += p.num_vertices;
                    kept++;
                }
                std::fclose(obj);
                std::fclose(mtl);
                std::printf("m2modeldump: %d polygons drawn nearer than %.1f: %s/drawn.obj\n", kept, limit,
                            drawn_dir.c_str());
                return 0;
            }
            if (list) {
                std::map<int32_t, int> counts;
                for (const auto &p : capture.polys) counts[p.object]++;
                std::vector<int> order(capture.objects.size());
                for (size_t i = 0; i < order.size(); i++) order[i] = int(i);
                std::sort(order.begin(), order.end(), [&](int a, int b) {
                    return std::fabs(capture.objects[a].matrix[11]) < std::fabs(capture.objects[b].matrix[11]);
                });
                std::printf("frame %" PRIu64 ": %zu objects, %zu polygons\n", game.board().frame(),
                            capture.objects.size(), capture.polys.size());
                std::printf("%4s %-10s %6s %6s %10s %10s %10s %4s %s\n", "draw", "address", "count", "polys", "x", "y",
                            "z", "win", "attr bits (link type, z sort, double sided): count");
                for (int i : order) {
                    const auto &o = capture.objects[i];
                    if (std::fabs(o.matrix[11]) > near_z) continue;
                    std::map<uint32_t, int> kinds; // what the object's polygons ask of the rasterizer
                    for (const auto &p : capture.polys)
                        if (p.object == i) kinds[(p.attr >> 8 & 3) | (p.attr >> 10 & 3) << 2 | (p.attr >> 17 & 1) << 4]++;
                    std::string k;
                    for (const auto &[bits, n] : kinds)
                        k += " l" + std::to_string(bits & 3) + "z" + std::to_string(bits >> 2 & 3) +
                             (bits & 16 ? "d" : "") + ":" + std::to_string(n);
                    std::printf("%4d %08x   %6u %6d %10.2f %10.2f %10.2f %4u%s\n", i, o.address, o.count, counts[i],
                                o.matrix[9], o.matrix[10], o.matrix[11], o.window, k.c_str());
                }
                continue;
            }
            // the reference: the first wanted object drawn this frame
            const rt::Geo::CaptureObject *ref = nullptr;
            for (const auto &o : capture.objects)
                if (o.address == wanted[0]) { ref = &o; break; }
            if (!ref) {
                std::fprintf(stderr, "m2modeldump: frame %" PRIu64 ": object %08x not drawn\n", game.board().frame(),
                             wanted[0]);
                continue;
            }
            const Frame to_ref(*ref);
            for (const auto &p : capture.polys) {
                const auto &o = capture.objects[p.object];
                if (std::find(wanted.begin(), wanted.end(), o.address) == wanted.end()) continue;
                if (((p.attr >> 8) & 3) == 0) continue; // link type 0: not drawn (check_culling)
                Face face{};
                face.n = p.num_vertices;
                std::vector<long long> key;
                for (int k = 0; k < face.n; k++) {
                    to_ref.to_model(p.view[k], face.p[k]);
                    for (int c = 0; c < 3; c++) key.push_back(std::llround(face.p[k][c] * 1000.0));
                    face.uv[k][0] = p.uv[k][0], face.uv[k][1] = p.uv[k][1];
                }
                std::memcpy(face.texheader, p.texheader, sizeof face.texheader);
                face.luma = p.luma;
                std::sort(key.begin(), key.end()); // the same face whatever its first corner
                if (seen.insert(key).second) faces.push_back(face);
            }
        }
        if (list) return 0;

        // OBJ: a vertex per corner (merged by position), a material per palette colour
        const std::string obj_path = out_dir + "/model.obj", mtl_path = out_dir + "/model.mtl";
        FILE *obj = std::fopen(obj_path.c_str(), "w");
        FILE *mtl = std::fopen(mtl_path.c_str(), "w");
        if (!obj || !mtl) {
            std::fprintf(stderr, "m2modeldump: cannot write in %s\n", out_dir.c_str());
            return 1;
        }
        std::fprintf(obj, "# m2modeldump: %zu faces, for personal use only\nmtllib model.mtl\n", faces.size());
        std::map<std::tuple<long long, long long, long long>, int> index;
        std::vector<std::array<double, 3>> verts;
        auto vertex = [&](const double p[3]) {
            const auto key = std::make_tuple(std::llround(p[0] * 1000.0), std::llround(p[1] * 1000.0),
                                             std::llround(p[2] * 1000.0));
            const auto it = index.find(key);
            if (it != index.end()) return it->second;
            verts.push_back({p[0], p[1], p[2]});
            return index[key] = int(verts.size());
        };
        std::vector<std::array<int, 4>> face_index(faces.size());
        for (size_t i = 0; i < faces.size(); i++)
            for (int k = 0; k < faces[i].n; k++) face_index[i][k] = vertex(faces[i].p[k]);
        for (const auto &v : verts) std::fprintf(obj, "v %.5f %.5f %.5f\n", v[0], v[1], v[2]);
        const uint8_t *palram = game.board().video().gpu_mem().palram;
        std::set<uint32_t> materials;
        for (size_t i = 0; i < faces.size(); i++) {
            const uint32_t colorbase = (faces[i].texheader[3] >> 6) & 0x3ff; // raster.cpp: the palette colour
            if (materials.insert(colorbase).second) {
                const uint16_t c = palram ? uint16_t(palram[(colorbase + 0x1000) * 2] | palram[(colorbase + 0x1000) * 2 + 1] << 8)
                                          : 0x7fff;
                std::fprintf(mtl, "newmtl c%03x\nKd %.3f %.3f %.3f\n\n", colorbase, ((c >> 0) & 31) / 31.0,
                             ((c >> 5) & 31) / 31.0, ((c >> 10) & 31) / 31.0);
            }
            std::fprintf(obj, "usemtl c%03x\nf", colorbase);
            for (int k = 0; k < faces[i].n; k++) std::fprintf(obj, " %d", face_index[i][k]);
            std::fprintf(obj, "\n");
        }
        std::fclose(obj);
        std::fclose(mtl);
        std::printf("m2modeldump: %zu faces, %zu vertices, %zu colours: %s\n", faces.size(), verts.size(),
                    materials.size(), obj_path.c_str());
    } catch (const std::exception &e) {
        std::fprintf(stderr, "m2modeldump: %s\n", e.what());
        return 1;
    }
    return 0;
}
