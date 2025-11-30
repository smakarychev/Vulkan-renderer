module diffuseIrradianceSH;

import "core/attributes";
import "core/lib";
import "light/light";

#define THREADGROUP_SIZE 8

struct Samplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct Resources {
    TextureCube environment;
    RWStructuredBuffer<SH2Irradiance> irradiance;
}

ParameterBlock<Samplers> samplers;
ParameterBlock<Resources> resources;

groupshared SH2RGB sSampledIrradiance[THREADGROUP_SIZE * THREADGROUP_SIZE];

SH2RGB sampleIrradiance(float3 direction, float mipIndex, float16_t weight) {
    const float16_t3 color = (float16_t3)resources.environment.SampleLevel(samplers.sampler, direction, mipIndex).rgb;

    return SH2.evaluateIrradiancePrenormalized((float16_t3)direction).multiplyRGB(color * weight);
}

void projectCommon(uint invocationId) {
    if (invocationId < 32)
        sSampledIrradiance[invocationId] = sSampledIrradiance[invocationId].add(sSampledIrradiance[invocationId + 32]);
    GroupMemoryBarrierWithGroupSync();

    if (invocationId < 16) 
        sSampledIrradiance[invocationId] = sSampledIrradiance[invocationId].add(sSampledIrradiance[invocationId + 16]);
    GroupMemoryBarrierWithGroupSync();

    if (invocationId < 8) 
        sSampledIrradiance[invocationId] = sSampledIrradiance[invocationId].add(sSampledIrradiance[invocationId + 8]);
    GroupMemoryBarrierWithGroupSync();
    
    if (invocationId < 4) 
        sSampledIrradiance[invocationId] = sSampledIrradiance[invocationId].add(sSampledIrradiance[invocationId + 4]);
    GroupMemoryBarrierWithGroupSync();
    
    if (invocationId < 2) 
        sSampledIrradiance[invocationId] = sSampledIrradiance[invocationId].add(sSampledIrradiance[invocationId + 2]);
    GroupMemoryBarrierWithGroupSync();
    
    if (invocationId < 1) {
        SH2RGB irradiance = sSampledIrradiance[invocationId].add(sSampledIrradiance[invocationId + 1]);
        resources.irradiance[0] = SH2Irradiance(irradiance);
    }
}

[shader("compute")]
[numthreads(THREADGROUP_SIZE, THREADGROUP_SIZE, 1)]
void projectRealtime(
    uint3 dispatchThreadID: SV_DispatchThreadID,
    uniform float mipIndex) {

    static const float UNIFORM_SAMPLE_SOLID_ANGLE = 4.0 * PI / (THREADGROUP_SIZE * THREADGROUP_SIZE);

    const uint invocationId = dispatchThreadID.x + dispatchThreadID.y * THREADGROUP_SIZE;
    float2 faceUv = float2(dispatchThreadID.xy + 0.5f) / float2(THREADGROUP_SIZE);
    const float3 sphere_sample = uniformHemisphereSample(faceUv);

    sSampledIrradiance[invocationId] = sampleIrradiance(sphere_sample, mipIndex, float16_t(UNIFORM_SAMPLE_SOLID_ANGLE));
    
    GroupMemoryBarrierWithGroupSync();
    projectCommon(invocationId);
}

[shader("compute")]
[numthreads(THREADGROUP_SIZE, THREADGROUP_SIZE, 1)]
void projectOffline(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform float mipIndex) {

    static const uint SAMPLES_PER_INVOCATION = 32;
    static const uint SAMPLES_TOTAL = SAMPLES_PER_INVOCATION * THREADGROUP_SIZE;
    static const float UNIFORM_SAMPLE_SOLID_ANGLE = 4.0 * PI / (SAMPLES_TOTAL * SAMPLES_TOTAL);

    const uint invocationId = dispatchThreadID.x + dispatchThreadID.y * THREADGROUP_SIZE;

    SH2RGB sampledIrradiance = SH2RGB.zero();

    for (uint v = 0; v < SAMPLES_PER_INVOCATION; v++) {
        for (uint u = 0; u < SAMPLES_PER_INVOCATION; u++) {
            float2 faceUv = 
                float2(dispatchThreadID.xy * SAMPLES_PER_INVOCATION + float2(u, v) + 0.5f) / float2(SAMPLES_TOTAL);
            const float3 sphere_sample = uniformSphereSample(faceUv);
            sampledIrradiance = sampledIrradiance.add(
                sampleIrradiance(sphere_sample, mipIndex, float16_t(UNIFORM_SAMPLE_SOLID_ANGLE)));
        }
    }

    sSampledIrradiance[invocationId] = sampledIrradiance;

    GroupMemoryBarrierWithGroupSync();
    projectCommon(invocationId);
}