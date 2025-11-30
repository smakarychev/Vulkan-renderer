module hizVisualize;

import "utility/fullscreen";
import "core/attributes";

struct HizVisualizeSamplers {
    [ImmutableSampler(SamplerFlags.Nearest)]
    SamplerState sampler;
}

struct HizVisualizeResources {
    Texture2D hiz;
}

ParameterBlock<HizVisualizeSamplers> samplers;
ParameterBlock<HizVisualizeResources> resources;

[shader("pixel")]
float4 main(FullScreenVSOutput input,
    uniform uint mipmap,
    uniform float intensity) {
    return float4(resources.hiz.SampleLevel(samplers.sampler, input.uv, mipmap).rrr * intensity, 1.0f);
}