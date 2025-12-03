module texture3dToSlice;

import "utility/fullscreen";
import "core/attributes";

struct Samplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct Resources {
    Texture3D texture;
}

ParameterBlock<Samplers> samplers;
ParameterBlock<Resources> resources;

[shader("pixel")]
float4 main(FullScreenVSOutput input,
    uniform float sliceNormalized) {
    return float4(resources.texture.SampleLevel(samplers.sampler, float3(input.uv, sliceNormalized), 0).rgb, 1.0f);
}