// CRT filter (launcher > CRT): the game's picture drawn as an arcade monitor
// shows it, scanlines, phosphor mask, glow and the tube's curvature, by
// crt.frag. Works for both renderers: the frame (at any super sampling) is
// first brought to its original resolution with mip levels, then drawn per
// screen pixel into the picture's rectangle.
//
// Shaders: SPIR-V (Vulkan) and MSL (Metal) from scripts/build_crt_shaders.py;
// none for Direct3D 12, where init() fails and the filter is off.
#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace app {

class CrtFilter {
public:
    struct Params {
        int mask = 2;              // 0 none, 1 aperture grille, 2 slot mask, 3 shadow mask
        float scanlines = 0.9f;    // 0 none .. 1 full beams
        float mask_strength = 0.35f;
        float glow = 0.12f;
        float curvature = 0.04f;
    };

    // Creates the pipeline for drawing into textures of `format` (the
    // swapchain's). False (with error()) where the shaders cannot run.
    bool init(SDL_GPUDevice *dev, SDL_GPUTextureFormat format);
    void shutdown();
    bool ok() const { return pipe_ != nullptr; }
    const std::string &error() const { return error_; }

    // Draws the picture (src at mip level `level`, its top-left src_w x src_h,
    // the game's w x h at some scale) into `dest` of `target`, clearing the
    // rest of target to black. Outside a render pass.
    void draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *src, Uint32 level, Uint32 src_w, Uint32 src_h, int w, int h,
              SDL_GPUTexture *target, const SDL_Rect &dest, const Params &params);

private:
    SDL_GPUDevice *dev_ = nullptr;
    SDL_GPUGraphicsPipeline *pipe_ = nullptr;
    SDL_GPUSampler *sampler_ = nullptr;
    SDL_GPUTexture *picture_ = nullptr; // the frame at its original resolution, with mip levels
    int picture_w_ = 0, picture_h_ = 0;
    std::string error_;
};

} // namespace app
