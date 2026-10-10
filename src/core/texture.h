#pragma once
#include <cstdint>

// Straight RGBA internally. Host adapters must preserve the host's alpha convention.
struct TextureParams {
    float grain = .025f, size = 1.2f, color = .15f;
    float halation = .15f, halRadius = 8.f, bloom = .08f, bloomRadius = 32.f;
    float threshold = .7f, softness = .2f;
    float shadows = .7f, highlights = .35f;
    float scale = 1.f; // rendered pixels per reference 1080-height image
    float originX = 0, originY = 0;
    uint32_t seed = 731, frame = 0;
    int linear = 0, particle = 0;
};

#ifdef TEXTURE_EXPORTS
#define FT_API extern "C" __declspec(dllexport)
#else
#define FT_API extern "C"
#endif
// Standalone interface: tightly packed, straight float RGBA. Returns 0 on success.
FT_API int ft_render(const float* input, float* output, int width, int height,
                     const TextureParams* params, float* kernel_ms);
FT_API const char* ft_error();

