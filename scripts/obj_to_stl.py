#!/usr/bin/env python3
"""Turn an OBJ from tools/m2modeldump into an STL ready to slice for a 3D
printer: polygons split into triangles, holes closed (each boundary loop
capped, its winding following the faces around it), flat pieces dropped (a
ground shadow, decals), the model stood on Z up and scaled to millimetres.

  obj_to_stl.py IN.obj OUT.stl [--scale 43] [--keep-flat] [--up y]

--scale N: 1:N (43 by default: a 5 m car is about 115 mm long). The game's
units are metres. --keep-flat: keep pieces thinner than 3 cm. --up: the
model's up axis (y, the game's, by default).

Standard library only. The STL is derived from the game: for your own use,
never shared (rules.md).
"""

import argparse
import math
import struct
from collections import defaultdict


def load(path):
    verts, faces = [], []
    for line in open(path):
        t = line.split()
        if not t:
            continue
        if t[0] == "v":
            verts.append(tuple(float(x) for x in t[1:4]))
        elif t[0] == "f":
            faces.append([int(x.split("/")[0]) - 1 for x in t[1:]])
    return verts, faces


def shells(faces):
    """Groups of faces joined by edges."""
    parent = list(range(len(faces)))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    by_edge = defaultdict(list)
    for i, f in enumerate(faces):
        for k in range(len(f)):
            a, b = f[k], f[(k + 1) % len(f)]
            by_edge[(min(a, b), max(a, b))].append(i)
    for fs in by_edge.values():
        for j in fs[1:]:
            parent[find(j)] = find(fs[0])
    groups = defaultdict(list)
    for i in range(len(faces)):
        groups[find(i)].append(i)
    return list(groups.values())


def cap_holes(verts, faces):
    """Close every boundary loop with a fan from its centre. A boundary edge
    a->b of a face is walked b->a by the cap, so the cap winds as its
    neighbours do."""
    directed = defaultdict(int)
    for f in faces:
        for k in range(len(f)):
            a, b = f[k], f[(k + 1) % len(f)]
            if a != b:
                directed[(a, b)] += 1
    nxt = {}
    for (a, b) in directed:
        if (b, a) not in directed:
            nxt[b] = a  # the cap's edge b -> a
    caps = 0
    while nxt:
        start = next(iter(nxt))
        loop, v = [], start
        while v in nxt and (not loop or v != start):
            loop.append(v)
            v = nxt.pop(v)
        if len(loop) < 3:
            continue
        c = tuple(sum(verts[i][k] for i in loop) / len(loop) for k in range(3))
        verts.append(c)
        ci = len(verts) - 1
        for i in range(len(loop)):
            faces.append([loop[i], loop[(i + 1) % len(loop)], ci])
        caps += 1
    return caps


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("obj")
    ap.add_argument("stl")
    ap.add_argument("--scale", type=float, default=43.0)
    ap.add_argument("--keep-flat", action="store_true")
    ap.add_argument("--up", choices="xyz", default="y")
    args = ap.parse_args()

    verts, faces = load(args.obj)
    kept = []
    for group in shells(faces):
        pts = [verts[v] for i in group for v in faces[i]]
        thin = min(max(p[k] for p in pts) - min(p[k] for p in pts) for k in range(3))
        if thin < 0.03 and not args.keep_flat:
            print(f"obj_to_stl: dropped a flat piece of {len(group)} faces ({thin * 100:.1f} cm thick)")
            continue
        kept += [faces[i] for i in group]
    faces = kept
    caps = cap_holes(verts, faces)
    if caps:
        print(f"obj_to_stl: closed {caps} hole(s)")

    # Z up, millimetres, standing on Z = 0, centred on X and Y
    up = "xyz".index(args.up)
    rest = [k for k in range(3) if k != up]
    s = 1000.0 / args.scale

    def place(p):  # the game's space is left-handed (x right, y up, z ahead): (x, z, y) is the same car, Z up
        return (p[rest[0]] * s, p[rest[1]] * s, p[up] * s)

    used = sorted({v for f in faces for v in f})
    pts = {v: place(verts[v]) for v in used}
    lo = [min(p[k] for p in pts.values()) for k in range(3)]
    hi = [max(p[k] for p in pts.values()) for k in range(3)]
    shift = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2])
    pts = {v: (p[0] - shift[0], p[1] - shift[1], p[2] - shift[2]) for v, p in pts.items()}

    tris = []
    for group in shells(faces):  # each piece wound so its normals face out (positive volume)
        piece = []
        for i in group:  # fans: Model 2 polygons are convex
            f = faces[i]
            for k in range(1, len(f) - 1):
                piece.append((pts[f[0]], pts[f[k]], pts[f[k + 1]]))
        volume = sum(a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) +
                     a[2] * (b[0] * c[1] - b[1] * c[0]) for a, b, c in piece) / 6.0
        tris += [(a, c, b) for a, b, c in piece] if volume < 0 else piece
    with open(args.stl, "wb") as out:
        out.write(b"m2modeldump obj_to_stl".ljust(80, b" "))
        out.write(struct.pack("<I", len(tris)))
        for a, b, c in tris:
            u = [b[k] - a[k] for k in range(3)]
            v = [c[k] - a[k] for k in range(3)]
            n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
            ln = math.sqrt(sum(x * x for x in n)) or 1.0
            out.write(struct.pack("<12fH", *(x / ln for x in n), *a, *b, *c, 0))
    size = [hi[k] - lo[k] for k in range(3)]
    print(f"obj_to_stl: {len(tris)} triangles, {size[0]:.1f} x {size[1]:.1f} x {size[2]:.1f} mm (1:{args.scale:g}): "
          f"{args.stl}")


if __name__ == "__main__":
    main()
