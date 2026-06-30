module equirectangularToCubemap;

import "core/attributes";
import "core/lib";

struct Samplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct Resources {
    Texture2D equirectangular;
    RWTexture2DArray<float4> cubemap;
}

ParameterBlock<Samplers> samplers;
ParameterBlock<Resources> resources;

[shader("compute")]
[numthreads(32, 32, 1)]
void computeMain(
    uint3 dispatchId : SV_DispatchThreadID,
    uniform float2 cubemapSizeInverse,
    uniform float exposure) {
    const float3 normal = getCubemapNormal(dispatchId, cubemapSizeInverse);

    const float phi = atan2(normal.z, normal.x);
    const float theta = acos(normal.y);
    
    const float2 uv = float2(phi / TAU, 1.0f - theta / PI);
    const float4 color = resources.equirectangular.SampleLevel(samplers.sampler, uv, 0) * exposure;
    resources.cubemap[dispatchId] = color;
}