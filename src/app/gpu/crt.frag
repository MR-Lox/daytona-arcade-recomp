// CRT filter (app/gpu/crt.h): the game's picture as an arcade monitor shows
// it, drawn per screen pixel from the picture at its original resolution
// (with mip levels, for the glow).
//
// In linear light: each scanline is an electron beam, its pixels blended
// along the line with a gaussian spot, and across the lines a gaussian whose
// width grows with brightness (bright lines bloom into the gap, dark ones are
// thin), each normalised to carry the same light whatever its width. Then the
// phosphor mask (aperture grille, slot mask or shadow mask) in screen pixels,
// with the light it blocks given back; the glass's halation (a wide blur of
// the picture); the tube's curvature, rounded corners and falloff.
//
// SDL_GPU resource sets (SPIR-V): fragment samplers set 2, fragment uniforms
// set 3. scripts/build_crt_shaders.py compiles it to SPIR-V and MSL.
#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;

layout(set = 2, binding = 0) uniform sampler2D picture;

layout(set = 3, binding = 0, std140) uniform CrtParams {
    vec4 size;  // xy: the picture in pixels; zw: its rectangle on screen, in pixels
    vec4 beam;  // x: scanlines 0-1; y, z: the beam's height (sigma, in lines) for black and for full
                // brightness; w: the spot's width along the line (sigma, in pixels)
    vec4 mask;  // x: 0 none, 1 aperture grille, 2 slot mask, 3 shadow mask; y: strength 0-1;
                // z: screen pixels per phosphor stripe; w: how much of the blocked light is given back
    vec4 tube;  // x: curvature; y: vignette; z: corner radius (fraction of the height); w: glow
};

vec3 to_linear(vec3 c) { return pow(max(c, vec3(0.0)), vec3(2.4)); }
vec3 to_display(vec3 c) { return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2)); }

// The beam along scanline y at x (in picture pixels): its pixels under a
// gaussian spot. Black above and below the picture.
vec3 scanline(int y, float x) {
    if (y < 0 || y >= int(size.y)) return vec3(0.0);
    const float x0 = floor(x - 0.5);
    const float k = -0.5 / (beam.w * beam.w);
    vec3 sum = vec3(0.0);
    float total = 0.0;
    for (int i = -1; i <= 2; i++) {
        const float px = x0 + float(i);
        const float d = px + 0.5 - x;
        const float w = exp(k * d * d);
        sum += to_linear(texelFetch(picture, ivec2(int(clamp(px, 0.0, size.x - 1.0)), y), 0).rgb) * w;
        total += w;
    }
    return sum / total;
}

// The phosphors at this screen pixel: each lets through its own colour, the
// rest by 1 - strength. Returns the mask and its average transmission.
vec3 phosphors(vec2 frag, out float average) {
    average = 1.0;
    const int type = int(mask.x + 0.5);
    if (type == 0 || mask.y <= 0.0) return vec3(1.0);
    const float dark = 1.0 - mask.y;
    vec3 m = vec3(dark);
    average = (1.0 + 2.0 * dark) / 3.0;
    if (type == 3) { // shadow mask: dots in triads, odd rows offset by half a triad
        const vec2 p = floor(frag / max(mask.z * 0.5, 0.5));
        const float row = floor(p.y / 2.0);
        const float i = mod(floor((p.x + 3.0 * mod(row, 2.0)) / 2.0), 3.0);
        m[int(i)] = 1.0;
        if (mod(p.y, 2.0) >= 1.0 && mask.z >= 2.0) m *= mix(1.0, dark, 0.5); // the gap between dot rows
        if (mask.z >= 2.0) average *= mix(1.0, dark, 0.5) * 0.5 + 0.5;
        return m;
    }
    const vec2 p = floor(frag / mask.z);
    m[int(mod(p.x, 3.0))] = 1.0; // aperture grille: red, green, blue stripes
    if (type == 2) { // slot mask: the stripes cut every 4 lines, staggered by triad
        const float triad = floor(p.x / 3.0);
        if (mod(p.y + 2.0 * mod(triad, 2.0), 4.0) < 1.0) m = vec3(dark);
        average *= (3.0 + dark) / 4.0;
    }
    return m;
}

void main() {
    // the tube's curvature: the picture's corners bend out of the glass
    vec2 c = v_uv * 2.0 - 1.0;
    c *= 1.0 + tube.x * c.yx * c.yx;
    const vec2 uv = c * 0.5 + 0.5;

    // rounded corners on the curved picture, antialiased over a screen pixel
    const vec2 half_px = 0.5 * size.zw;
    const float radius = tube.z * size.w;
    const vec2 q = abs(c) * half_px - (half_px - radius);
    const float edge = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
    const float inside = clamp(0.5 - edge, 0.0, 1.0);
    if (inside <= 0.0) {
        o_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // the beams: four scanlines around this point, each a gaussian across
    const vec2 p = uv * size.xy;
    const int r = int(floor(p.y - 0.5));
    vec3 lines[4];
    vec3 beams = vec3(0.0);
    for (int i = 0; i < 4; i++) {
        const int y = r - 1 + i;
        lines[i] = scanline(y, p.x);
        const float d = p.y - (float(y) + 0.5);
        // never thinner than about a screen pixel, or small windows show moire
        const vec3 sigma = max(mix(vec3(beam.y), vec3(beam.z), sqrt(lines[i])), vec3(0.45 * size.y / size.w));
        beams += lines[i] * exp(-0.5 * d * d / (sigma * sigma)) / (sigma * 2.5066283);
    }
    const vec3 flat_color = mix(lines[1], lines[2], clamp(p.y - 0.5 - float(r), 0.0, 1.0));
    vec3 color = mix(flat_color, beams, beam.x);

    float average;
    const vec3 m = phosphors(gl_FragCoord.xy, average);
    color *= m * mix(1.0, 1.0 / average, mask.w);

    // halation: light scattered in the glass, a wide blur of the picture
    if (tube.w > 0.0) {
        const vec2 t = 2.0 / size.xy;
        vec3 glow = to_linear(textureLod(picture, uv, 2.0).rgb) * 2.0;
        glow += to_linear(textureLod(picture, uv + vec2(t.x, 0.0), 2.0).rgb);
        glow += to_linear(textureLod(picture, uv - vec2(t.x, 0.0), 2.0).rgb);
        glow += to_linear(textureLod(picture, uv + vec2(0.0, t.y), 2.0).rgb);
        glow += to_linear(textureLod(picture, uv - vec2(0.0, t.y), 2.0).rgb);
        glow = glow / 6.0 * 0.6 + to_linear(textureLod(picture, uv, 4.0).rgb) * 0.4;
        color += glow * tube.w;
    }

    // the tube's falloff towards the edges
    const vec2 v = clamp(uv, 0.0, 1.0);
    color *= pow(clamp(16.0 * v.x * (1.0 - v.x) * v.y * (1.0 - v.y), 0.0, 1.0), tube.y);

    o_color = vec4(to_display(color) * inside, 1.0);
}
