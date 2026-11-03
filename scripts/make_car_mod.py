#!/usr/bin/env python3
"""Make a custom player car (.m2car, runtime/car_mod.h) from an OBJ model of
your own: the launcher's Game tab > Custom car draws it in place of the
player's car (Revision A).

  make_car_mod.py IN.obj OUT.m2car [--length 3.05] [--forward +x] [--up +y]
                  [--wheels REGEX] [--skip REGEX] [--wheels-from OTHER.obj]

--length: the car's real length in metres (the model is scaled to it).
--forward, --up: the model's axes (+x, -z, ...); its right is forward x up,
as in any right-handed OBJ (Blender, Fusion, Maya).
--wheels: the OBJ groups (g/o names) that are wheels, a regular expression
(by default w-, wheel, roue, pneu, tyre, tire, jante, rim); they make the four
wheels, by corner: their centres and sizes place the game's wheels (or these
wheels themselves) under the car. --skip: groups to leave out (in both
files). --wheels-from: the wheels from another OBJ in the same coordinates
(the same model exported with its wheels), for a model exported without them.

The car stands on the game's ground, its wheels centred under the game's car
(their midpoint between the axles at the game's). Colours: each material's
Kd, in the game's 15-bit colour. Standard library only.
"""

import argparse
import math
import re
import sys

AXES = {"+x": (1, 0, 0), "-x": (-1, 0, 0), "+y": (0, 1, 0), "-y": (0, -1, 0), "+z": (0, 0, 1), "-z": (0, 0, -1)}
WHEELS = r"(^|[^a-z])(w-|wheel|roue|pneu|tyre|tire|jante|rim)"
GROUND = -0.03      # the game car's ground in its body space (its wheels' bottoms)
AXLES_MID = 0.0075  # the game car's midpoint between its axles (z)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def load(path):
    verts, normals, faces, materials = [], [], [], {}
    group, material = "", ""
    base = path.rsplit("/", 1)[0] if "/" in path else "."
    for line in open(path, errors="replace"):
        t = line.split()
        if not t:
            continue
        if t[0] == "v":
            verts.append(tuple(float(x) for x in t[1:4]))
        elif t[0] == "vn":
            normals.append(tuple(float(x) for x in t[1:4]))
        elif t[0] in ("g", "o"):
            group = " ".join(t[1:])
        elif t[0] == "usemtl":
            material = " ".join(t[1:])
        elif t[0] == "mtllib":
            name = line.split(None, 1)[1].strip()
            try:
                current = None
                for m in open(base + "/" + name, errors="replace"):
                    u = m.split()
                    if u and u[0] == "newmtl":
                        current = " ".join(u[1:])
                    elif u and u[0] == "Kd" and current is not None:
                        materials[current] = tuple(float(x) for x in u[1:4])
            except OSError:
                print(f"make_car_mod: no {name}: every face light grey", file=sys.stderr)
        elif t[0] == "f":
            vi, ni = [], []
            for c in t[1:]:
                parts = c.split("/")
                i = int(parts[0])
                vi.append(i - 1 if i > 0 else len(verts) + i)
                if len(parts) > 2 and parts[2]:
                    j = int(parts[2])
                    ni.append(j - 1 if j > 0 else len(normals) + j)
            faces.append((vi, ni if len(ni) == len(vi) else None, group, material))
    return verts, normals, faces, materials


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("obj")
    ap.add_argument("out")
    ap.add_argument("--length", type=float, default=3.05)
    ap.add_argument("--forward", choices=AXES, default="+x")
    ap.add_argument("--up", choices=AXES, default="+y")
    ap.add_argument("--wheels", default=WHEELS)
    ap.add_argument("--skip", default=None)
    ap.add_argument("--wheels-from", default=None)
    args = ap.parse_args()

    forward, up = AXES[args.forward], AXES[args.up]
    if dot(forward, up) != 0:
        sys.exit("make_car_mod: --forward and --up must be different axes")
    right = cross(forward, up)
    verts, normals, faces, materials = load(args.obj)
    wheel_re, skip_re = re.compile(args.wheels, re.I), re.compile(args.skip, re.I) if args.skip else None
    if args.wheels_from:  # its wheel groups join the model, their indices after the model's
        wv, wn, wf, wm = load(args.wheels_from)
        nv, nn = len(verts), len(normals)
        verts += wv
        normals += wn
        faces = [f for f in faces if not wheel_re.search(f[2])]
        faces += [([i + nv for i in vi], [j + nn for j in ni] if ni else None, "wheels-from " + g, "wf " + m)
                  for vi, ni, g, m in wf if wheel_re.search(g)]
        materials.update({"wf " + k: v for k, v in wm.items()})
    faces = [f for f in faces if not (skip_re and skip_re.search(f[2]))]
    if not faces:
        sys.exit("make_car_mod: no faces")

    # the game's axes (x right, y up, z ahead), then metres
    game = [(dot(v, right), dot(v, up), dot(v, forward)) for v in verts]
    used = {i for f in faces for i in f[0]}
    zs = [game[i][2] for i in used]
    scale = args.length / (max(zs) - min(zs))
    game = [(p[0] * scale, p[1] * scale, p[2] * scale) for p in game]

    # the wheels: groups by corner (left/right of the wheels' middle, ahead/behind)
    wheel_faces = [f for f in faces if wheel_re.search(f[2])]
    if not wheel_faces:
        sys.exit("make_car_mod: no wheel groups (see --wheels)")
    wv = {i for f in wheel_faces for i in f[0]}
    mid_x = (max(game[i][0] for i in wv) + min(game[i][0] for i in wv)) / 2
    mid_z = (max(game[i][2] for i in wv) + min(game[i][2] for i in wv)) / 2
    corners = {c: [] for c in ("FL", "FR", "RL", "RR")}
    for f in wheel_faces:
        c = [game[i] for i in f[0]]
        cx, cz = sum(p[0] for p in c) / len(c), sum(p[2] for p in c) / len(c)
        corners[("F" if cz > mid_z else "R") + ("R" if cx > mid_x else "L")].append(f)
    wheels = {}
    for name, fs in corners.items():
        if not fs:
            sys.exit(f"make_car_mod: no wheel at {name}")
        pts = [game[i] for f in fs for i in f[0]]
        lo = [min(p[k] for p in pts) for k in range(3)]
        hi = [max(p[k] for p in pts) for k in range(3)]
        wheels[name] = [(lo[k] + hi[k]) / 2 for k in range(3)] + [max(hi[1] - lo[1], hi[2] - lo[2]) / 2]

    # stand it on the game's ground, its axles' midpoint at the game's, centred
    bottom = min(w[1] - w[3] for w in wheels.values())
    axles = (wheels["FL"][2] + wheels["FR"][2] + wheels["RL"][2] + wheels["RR"][2]) / 4
    shift = (-mid_x, GROUND - bottom, AXLES_MID - axles)
    game = [(p[0] + shift[0], p[1] + shift[1], p[2] + shift[2]) for p in game]
    for w in wheels.values():
        for k in range(3):
            w[k] += shift[k]

    def color(material):
        r, g, b = materials.get(material, (0.8, 0.8, 0.8))
        q = lambda x: max(0, min(31, round(x * 31)))
        return q(r) | q(g) << 5 | q(b) << 10

    def triangles(fs):
        out = []
        for vi, ni, _, material in fs:
            # the face's outward normal: its winding's (OBJ space), turned to agree with its vertex normals
            n = (0.0, 0.0, 0.0)
            for k in range(1, len(vi) - 1):
                c = cross(sub(verts[vi[k]], verts[vi[0]]), sub(verts[vi[k + 1]], verts[vi[0]]))
                n = (n[0] + c[0], n[1] + c[1], n[2] + c[2])
            if ni:
                avg = [sum(normals[j][k] for j in ni) for k in range(3)]
                if dot(n, avg) < 0:
                    n = (-n[0], -n[1], -n[2])
            ln = math.sqrt(dot(n, n))
            if ln == 0:
                continue
            gn = (dot(n, right) / ln, dot(n, up) / ln, dot(n, forward) / ln)
            col = color(material)
            for k in range(1, len(vi) - 1):
                tri = (game[vi[0]], game[vi[k]], game[vi[k + 1]])
                c = cross(sub(tri[1], tri[0]), sub(tri[2], tri[0]))
                if dot(c, c) < 1e-14:
                    continue  # degenerate
                out.append(tri + (gn, col))
        return out

    body = triangles([f for f in faces if not wheel_re.search(f[2])])
    with open(args.out, "w") as o:
        o.write("m2car 1\n# made by make_car_mod.py from %s: metres, x right, y up, z ahead\n" % args.obj.rsplit("/", 1)[-1])
        for name in ("FL", "FR", "RL", "RR"):
            o.write("wheel %s %.4f %.4f %.4f %.4f\n" % (name, *wheels[name]))

        def write(header, tris):
            o.write("%s %d\n" % (header, len(tris)))
            for a, b, c, n, col in tris:
                o.write("%.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.3f %.3f %.3f %d\n" % (*a, *b, *c, *n, col))

        write("body", body)
        for name in ("FL", "FR", "RL", "RR"):
            write("wheelmesh " + name, triangles(corners[name]))
    size = [max(p[k] for p in (game[i] for i in used)) - min(p[k] for p in (game[i] for i in used)) for k in range(3)]
    print(f"make_car_mod: {len(body)} body triangles, {sum(len(v) for v in corners.values())} wheel faces; "
          f"{size[0]:.2f} x {size[1]:.2f} x {size[2]:.2f} m; wheels "
          + ", ".join(f"{n} r {wheels[n][3]:.3f}" for n in wheels) + f": {args.out}")


if __name__ == "__main__":
    main()
