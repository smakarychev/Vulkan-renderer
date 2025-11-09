module hizCombined;

import "core/attributes";

#ifndef HIZ_MIN_MAX
#define HIZ_MIN_MAX 1
#endif // HIZ_MIN_MAX

#ifndef HIZ_MIN_MAX_DEPTH_BUFFER
#define HIZ_MIN_MAX_DEPTH_BUFFER 1
#endif // HIZ_MIN_MAX_DEPTH_BUFFER

struct HizSamplers {
    [ImmutableSampler(SamplerFlags.ReductionMin)]
    SamplerState minSampler;
    [ImmutableSampler(SamplerFlags.ReductionMax)]
    SamplerState maxSampler;
}

static const uint MAX_MIP_MAPS = 16;

struct MinMaxBuffer {
    uint min;
    uint max;
}

struct HizResources {
    Texture2D input;
#if HIZ_MIN_MAX
    RWTexture2D<float2> output[MAX_MIP_MAPS];
#else  // HIZ_MIN_MAX
    RWTexture2D<float> output[MAX_MIP_MAPS];
#endif // HIZ_MIN_MAX
    RWStructuredBuffer<MinMaxBuffer> minMax;
}

ParameterBlock<HizSamplers> samplers;
ParameterBlock<HizResources> resources;

int2 levelSize(int level) {
    int2 dimesions;
    resources.output[level].GetDimensions(dimesions.x, dimesions.y);
    return dimesions;
}
#define NVPRO_PYRAMID_LEVEL_SIZE levelSize

#if HIZ_MIN_MAX
#define NVPRO_PYRAMID_TYPE float2
void store(in int2 coord, in int level, in float2 in_) {
    resources.output[level][coord] = in_;
}
float2 reduce4(float2 v00, float2 v01, float2 v10, float2 v11) {
    float2 reduced;
    reduced.x = min(min(v00.x, v01.x), min(v10.x, v11.x));
    reduced.y = max(max(v00.y, v01.y), max(v10.y, v11.y));

    return reduced;
}
void loadReduce4(in int2 coord, in int level, out float2 val) {
    float2 normalizedCoord = (float2(coord) + float2(1)) / levelSize(level);
    val.x = resources.input.SampleLevel(samplers.minSampler, normalizedCoord, level).r;
    val.y = resources.input.SampleLevel(samplers.maxSampler, normalizedCoord, level).g;
}
#else // HIZ_MIN_MAX
#define NVPRO_PYRAMID_TYPE float
void store(in int2 coord, in int level, in float val) {
    resources.output[level][coord] = val;
}
float reduce4(float v00, float v01, float v10, float v11) {
    return min(min(v00, v01), min(v10, v11));
}
void loadReduce4(in int2 coord, in int level, out float val) {
    float2 normalized_coord = (float2(coord) + float2(1)) / levelSize(level);
    val = resources.input.SampleLevel(samplers.minSampler, normalized_coord, level).r;
}
#endif // HIZ_MIN_MAX

#define NVPRO_PYRAMID_STORE(coord, level, in_) store(coord, level, in_)

#define NVPRO_PYRAMID_REDUCE4(v00, v01, v10, v11, out_) \
    out_ = reduce4(v00, v01, v10, v11)

#define NVPRO_PYRAMID_LOAD_REDUCE4 loadReduce4

[shader("compute")]
[numthreads(8, 8, 1)]
void blitPass(int2 dispatchId : SV_DispatchThreadID) {
    const float2 imageSizeInverse = 1.0f / levelSize(0);
    const float2 uv = float2(dispatchId + 0.5f) * imageSizeInverse;
    
    float2 depthSample;
    depthSample.r = resources.input.SampleLevel(samplers.minSampler, uv, 0).r;
#if HIZ_MIN_MAX
    depthSample.g = resources.input.SampleLevel(samplers.maxSampler, uv, 0).r;
#if HIZ_MIN_MAX_DEPTH_BUFFER
    
    if (depthSample.r != 0) {
        const float min = WaveActiveMin(depthSample.r);
        const float max = WaveActiveMax(depthSample.g);
        
        if (WaveIsFirstLane()) {
            InterlockedMin(resources.minMax[0].min, asuint(min));
            InterlockedMax(resources.minMax[0].max, asuint(max));
        }
    }
#endif // HIZ_MIN_MAX_DEPTH_BUFFER

    resources.output[0][dispatchId] = depthSample.rg;
#else // HIZ_MIN_MAX
    resources.output[0][dispatchId] = depthSample.r;
#endif // HIZ_MIN_MAX
}

#define NVPRO_PYRAMID_IS_FAST_PIPELINE 1
#include "nvpro/nvproPyramid.slang"
[shader("compute")]
[numthreads(WORK_GROUP_X, 1, 1)]
void generatePass(uint2 globalInvocationId : SV_DispatchThreadID, uint localInvocationIndex : SV_GroupIndex,
    uint3 workgroupId : SV_GroupID) {
    nvproPyramidMain(globalInvocationId, localInvocationIndex, workgroupId);
}