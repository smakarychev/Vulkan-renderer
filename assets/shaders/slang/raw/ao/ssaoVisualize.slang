module ssaoVisualize;

import "utility/fullscreen";
import "core/attributes";

struct SsaoVisualizeSamplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct SsaoVisualizeResources {
    Texture2D ssao;
}

ParameterBlock<SsaoVisualizeSamplers> samplers;
ParameterBlock<SsaoVisualizeResources> resources;

[shader("pixel")]
float4 main(FullScreenVSOutput input) {
    return float4(resources.ssao.SampleLevel(samplers.sampler, input.UV, 0).rrr, 1.0f);
}