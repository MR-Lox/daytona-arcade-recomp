#include "app/gpu/crt.h"

#include "app/gpu/crt_gen.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace app {

namespace {

struct Uniforms { // crt.frag CrtParams (std140: four vec4)
    float size[4], beam[4], mask[4], tube[4];
};

} // namespace

bool CrtFilter::init(SDL_GPUDevice *dev, SDL_GPUTextureFormat format) {
    dev_ = dev;
    const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(dev_);
    auto shader = [&](const unsigned char *spv, size_t spv_len, const char *msl, SDL_GPUShaderStage stage, int samplers,
                      int uniforms) -> SDL_GPUShader * {
        SDL_GPUShaderCreateInfo si{};
        si.stage = stage;
        si.num_samplers = Uint32(samplers);
        si.num_uniform_buffers = Uint32(uniforms);
        if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
            si.format = SDL_GPU_SHADERFORMAT_SPIRV, si.code = spv, si.code_size = spv_len, si.entrypoint = "main";
        } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
            si.format = SDL_GPU_SHADERFORMAT_MSL, si.code = reinterpret_cast<const Uint8 *>(msl),
            si.code_size = std::strlen(msl), si.entrypoint = "main0";
        } else {
            error_ = "no CRT shaders for this graphics API (Vulkan or Metal only)";
            return nullptr;
        }
        SDL_GPUShader *s = SDL_CreateGPUShader(dev_, &si);
        if (!s) error_ = std::string("SDL_CreateGPUShader: ") + SDL_GetError();
        return s;
    };
    SDL_GPUShader *vs = shader(k_crt_vs_spv, sizeof k_crt_vs_spv, k_crt_vs_msl, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
    SDL_GPUShader *ps = vs ? shader(k_crt_ps_spv, sizeof k_crt_ps_spv, k_crt_ps_msl, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1)
                           : nullptr;
    if (vs && ps) {
        SDL_GPUColorTargetDescription color{};
        color.format = format;
        SDL_GPUGraphicsPipelineCreateInfo pi{}; // no vertex buffer: one triangle from the vertex index
        pi.vertex_shader = vs;
        pi.fragment_shader = ps;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.target_info.color_target_descriptions = &color;
        pi.target_info.num_color_targets = 1;
        pipe_ = SDL_CreateGPUGraphicsPipeline(dev_, &pi);
        if (!pipe_) error_ = std::string("SDL_CreateGPUGraphicsPipeline: ") + SDL_GetError();
    }
    if (vs) SDL_ReleaseGPUShader(dev_, vs);
    if (ps) SDL_ReleaseGPUShader(dev_, ps);
    if (!pipe_) return false;

    SDL_GPUSamplerCreateInfo smp{}; // texel fetches for the beams; filtered mip levels for the glow
    smp.min_filter = smp.mag_filter = SDL_GPU_FILTER_LINEAR;
    smp.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    smp.address_mode_u = smp.address_mode_v = smp.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    smp.max_lod = 16.0f;
    sampler_ = SDL_CreateGPUSampler(dev_, &smp);
    if (!sampler_) {
        error_ = std::string("SDL_CreateGPUSampler: ") + SDL_GetError();
        shutdown();
        return false;
    }
    return true;
}

void CrtFilter::shutdown() {
    if (!dev_) return;
    if (pipe_) SDL_ReleaseGPUGraphicsPipeline(dev_, pipe_), pipe_ = nullptr;
    if (sampler_) SDL_ReleaseGPUSampler(dev_, sampler_), sampler_ = nullptr;
    if (picture_) SDL_ReleaseGPUTexture(dev_, picture_), picture_ = nullptr;
    picture_w_ = picture_h_ = 0;
}

void CrtFilter::draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *src, Uint32 level, Uint32 src_w, Uint32 src_h, int w,
                     int h, SDL_GPUTexture *target, const SDL_Rect &dest, const Params &params) {
    if (!ok()) return;
    if (w != picture_w_ || h != picture_h_) { // the picture's size (widescreen changes the width)
        if (picture_) SDL_ReleaseGPUTexture(dev_, picture_);
        SDL_GPUTextureCreateInfo ti{};
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        ti.width = Uint32(w);
        ti.height = Uint32(h);
        ti.layer_count_or_depth = 1;
        ti.num_levels = Uint32(1 + std::floor(std::log2(double(std::max(w, h)))));
        picture_ = SDL_CreateGPUTexture(dev_, &ti);
        picture_w_ = picture_ ? w : 0, picture_h_ = picture_ ? h : 0;
        if (!picture_) return;
    }

    // the frame at its original resolution (a super sampled one averaged down), then its mip levels
    SDL_GPUBlitInfo blit{};
    blit.source.texture = src;
    blit.source.mip_level = level;
    blit.source.w = src_w;
    blit.source.h = src_h;
    blit.destination.texture = picture_;
    blit.destination.w = Uint32(w);
    blit.destination.h = Uint32(h);
    blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
    blit.filter = SDL_GPU_FILTER_LINEAR;
    SDL_BlitGPUTexture(cmd, &blit);
    SDL_GenerateMipmapsForGPUTexture(cmd, picture_);

    // one phosphor stripe per screen pixel or more: about 1.2 triads per picture pixel, as an arcade monitor
    const float stripe = std::max(1.0f, std::round(float(dest.w) / (float(w) * 3.0f * 1.2f)));
    const Uniforms u = {
        {float(w), float(h), float(dest.w), float(dest.h)},
        {std::clamp(params.scanlines, 0.0f, 1.0f), 0.17f, 0.42f, 0.4f},
        {float(std::clamp(params.mask, 0, 3)), std::clamp(params.mask_strength, 0.0f, 1.0f), stripe, 0.85f},
        {std::clamp(params.curvature, 0.0f, 0.25f), 0.12f, params.curvature > 0 ? 0.03f : 0.0f,
         std::clamp(params.glow, 0.0f, 1.0f)},
    };
    SDL_PushGPUFragmentUniformData(cmd, 0, &u, sizeof u);

    SDL_GPUColorTargetInfo ct{};
    ct.texture = target;
    ct.load_op = SDL_GPU_LOADOP_CLEAR;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    ct.clear_color = SDL_FColor{0, 0, 0, 1};
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
    const SDL_GPUViewport viewport{float(dest.x), float(dest.y), float(dest.w), float(dest.h), 0, 1};
    SDL_SetGPUViewport(pass, &viewport);
    SDL_SetGPUScissor(pass, &dest);
    SDL_BindGPUGraphicsPipeline(pass, pipe_);
    const SDL_GPUTextureSamplerBinding binding{picture_, sampler_};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

} // namespace app
