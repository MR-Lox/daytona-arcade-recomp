// CRT filter (app/gpu/crt.h): one triangle covering the viewport (the picture's
// rectangle on screen), uv 0..1 from its top-left. scripts/build_crt_shaders.py
// compiles it to SPIR-V and MSL (src/app/gpu/crt_gen.h).
#version 450

layout(location = 0) out vec2 v_uv;

void main() {
    const vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2); // (0,0) (2,0) (0,2)
    v_uv = p;
    gl_Position = vec4(p.x * 2.0 - 1.0, 1.0 - p.y * 2.0, 0.0, 1.0);
}
