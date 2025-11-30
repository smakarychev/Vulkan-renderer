module environmentPrefilter;

import "core/attributes";
import "core/lib";
import "pbr";

struct Samplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct Resources {
    TextureCube environment;
    RWTexture2DArray<float4> prefiltered;
}

[SpecializationConstant] const bool REAL_TIME = false;

ParameterBlock<Samplers> samplers;
ParameterBlock<Resources> resources;

float4 prefilter(float3 normal, uint n, float2 environmentSizeInverse, float roughness,) {
    float3 prefilter = float3(0.0f);
    float totalWeight = 0.0f;
    for (uint i = 0; i < n; i++) {
        const float2 hammersley = hammersley2d(i, n);
        const float3 sampleDir = importanceGgxHemisphereSample(hammersley, normal, roughness);
        const float3 l = normalize(2.0f * dot(normal, sampleDir) * sampleDir - normal);
        const float NoL = saturate(dot(normal, l));
        
        if (NoL > 0.0f) {
            const float NoH = clamp(dot(normal, sampleDir), 0.0f, 1.0f);
            const float VoH = NoH;
            const float d = ggxNormalDistribution(NoH, roughness);
            float pdf = d * NoH / (4.0f * VoH) + 1e-4f;
            float saTexel = 4.0f * PI * environmentSizeInverse.x * environmentSizeInverse.x / 6.0f;
            float saSample = 1.0f / (float(n) * pdf + 1e-4f);
            
            float mipLevel = roughness > 0.0f ? 0.5f * log2(saSample / saTexel) : 0.0f;
            prefilter += resources.environment.SampleLevel(samplers.sampler, l, mipLevel).rgb * NoL;
            totalWeight += NoL;
        }
    }
    prefilter /= totalWeight;
    
    return float4(prefilter, 1.0f);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(
    uint3 dispatchThreadID: SV_DispatchThreadID,
    uniform float2 prefilterSizeInverse,
    uniform float2 environmentSizeInverse,
    uniform float roughness) {

    int3 imageSize;
    resources.prefiltered.GetDimensions(imageSize.x, imageSize.y, imageSize.z);
    if (dispatchThreadID.x >= imageSize.x || dispatchThreadID.y >= imageSize.y)
        return;

    const float3 normal = getCubemapNormal(dispatchThreadID, prefilterSizeInverse);
    uint samples;
    if (REAL_TIME)
        samples = roughness < 0.1f ? 32 : 64;
    else
        samples = 1024;

    resources.prefiltered[dispatchThreadID] = prefilter(normal, samples, environmentSizeInverse, roughness);
}