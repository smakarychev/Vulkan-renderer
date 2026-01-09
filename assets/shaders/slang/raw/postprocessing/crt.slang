module crt;

import "core/attributes";
import "utility/fullscreen";

struct Samplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct Resources {
    Texture2D image;
    ConstantBuffer<float> time;

    struct Settings {
        float curvature;
        float colorSplit;
        float linesMultiplier;
        float vignettePower;
        float vignetteRadius;
    }
    ConstantBuffer<Settings> settings;
}

ParameterBlock<Samplers> samplers;
ParameterBlock<Resources> resources;

float2 uvTransform(float2 uv, float2 curvature) {
    uv = uv * 2.0f - 1.0f;
    float2 offset = abs(uv.yx) * float2(curvature.x, curvature.y);
    uv = uv + uv * offset * offset;
    uv = uv * 0.5f + 0.5f;

    return uv;
}

float3 scanline(float3 color, float2 uv) {
    float scanline = clamp(0.95f + 0.05f * cos(3.14f * (-uv.y + 0.008f * resources.time) * 240.0f * 1.0f), 0.0f, 1.0f);       
    float grille = 0.85f + 0.15f * clamp(1.5f * cos(3.14f * uv.x * 640.0f * 1.0f), 0.0f, 1.0f);
    color *= scanline * grille * 1.2f;

    return color;
}

float3 vignette(float3 color, float2 uv, float2 imageSize) {
    
    uv *= 1.0f - uv.yx;
    float vignette = uv.x * uv.y * resources.settings.vignetteRadius * imageSize.x;
    vignette = clamp(pow(vignette, resources.settings.vignettePower), 0.0f, 1.0f);
    color *= vignette;

    return color;
}

float3 splitColors(float2 uv, float split) {
    float2 uvr = uv + float2(0.0f, split);
    if (uvr.y > 1.0f) uvr = uv;
    float2 uvb = uv - float2(0.0f, split);
    if (uvb.y < 0.0f) uvb = uv;

    float3 color;
    color.r = resources.image.SampleLevel(samplers.sampler, uvr, 0).r;
    color.g = resources.image.SampleLevel(samplers.sampler, uv, 0).g;
    color.b = resources.image.SampleLevel(samplers.sampler, uvb, 0).b;

    return color;
}

[shader("pixel")]
float4 crtMain(FullScreenVSOutput input) {
    float2 uv = uvTransform(input.uv, float2(resources.settings.curvature));
    float2 imageSize;
    resources.image.GetDimensions(imageSize.x, imageSize.y);

    if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1) {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float3 color = splitColors(uv, resources.settings.colorSplit);
    color = vignette(color, uv, imageSize);
    //// loophero
    color.g *= (sin(uv.y * imageSize.y * resources.settings.linesMultiplier) + 1.0f) * 0.15f + 1.0f;
    color.rb *= (cos(uv.y * imageSize.y * resources.settings.linesMultiplier) + 1.0f) * 0.135f + 1.0f;
    
    return float4(color, 1.0f);
}
