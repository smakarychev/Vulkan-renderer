module ssao;

//https://wickedengine.net/2019/09/22/improved-normal-reconstruction-from-depth/

import "core/attributes";
import "core/lib";
import "core/viewInfo";

#ifndef MAX_SAMPLES
#define MAX_SAMPLES 128
#endif // MAX_SAMPLES

static const uint GROUP_SIZE = 16;
static const uint TILE_SIZE = GROUP_SIZE + 2;

groupshared float2 sPositions[TILE_SIZE * TILE_SIZE];
groupshared float sDepths[TILE_SIZE * TILE_SIZE];

struct Settings {
    float power;
    float radius;
    uint samples;
}

struct SsaoSamplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState depthSampler;
    [ImmutableSampler(SamplerFlags.Nearest)]
    SamplerState noiseSampler;
}

struct SsaoResources {
    Texture2D noise;
    Texture2D depth;
    RWTexture2D<float> ssao;
    ConstantBuffer<float4[MAX_SAMPLES]> samples;
    ConstantBuffer<ViewInfo, ScalarDataLayout> view;
    ConstantBuffer<Settings> settings;
}

ParameterBlock<SsaoSamplers> samplers;
ParameterBlock<SsaoResources> resources;

uint2 unflatten2d(uint index, uint size) {
    return uint2(index % size, index / size); 
}

uint flatten2d(uint2 coords, uint size) {
    return coords.x + coords.y * size;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uint2 workGroupID: SV_GroupID,
    uint localInvocationIndex: SV_GroupIndex,
    int2 localInvocationID: SV_GroupThreadID,
    uniform float2 ssaoSizeInverse,
    uniform float2 ssaoSize,
    uniform float2 noiseSizeInverse) {
    
    const int2 topLeft = int2(workGroupID * GROUP_SIZE) - 1;
    // we have a total of GROUP_SIZE * GROUP_SIZE threads, but each thread needs to access the surrounding texels,
    // which means that we have to calculate a total of (GROUP_SIZE + 1) * (GROUP_SIZE + 1) values,
    // the for loop below is a very simple way of splitting the work somewhat evenly across the available threads
    for (uint i = localInvocationIndex; i < TILE_SIZE * TILE_SIZE; i += GROUP_SIZE * GROUP_SIZE) {
        const uint2 pixelCoords = topLeft + unflatten2d(i, TILE_SIZE);
        const float2 uv = (float2(pixelCoords) + 0.5f) * ssaoSizeInverse;
        const float depth = resources.depth.SampleLevel(samplers.depthSampler, uv, 0).r;
        const float3 position = reconstructPosition(uv, depth, resources.view.camera.inverseProjection);
        
        sPositions[i] = position.xy;
        sDepths[i] = position.z;
    }
    GroupMemoryBarrierWithGroupSync();
    
    // ids of neighbouring pixels + pixel itself
    const uint ids[5] = {
        flatten2d(1u + localInvocationID.xy + int2( 0, 0), TILE_SIZE), // center
        flatten2d(1u + localInvocationID.xy + int2( 1, 0), TILE_SIZE), // right
        flatten2d(1u + localInvocationID.xy + int2(-1, 0), TILE_SIZE), // left
        flatten2d(1u + localInvocationID.xy + int2( 0, 1), TILE_SIZE), // bottom
        flatten2d(1u + localInvocationID.xy + int2( 0,-1), TILE_SIZE), // top
    };
    
    const float centerDepth = sDepths[ids[0]];
    if (centerDepth >= resources.view.camera.far)
        return;
    
    const uint bestHorizontal = abs(sDepths[ids[1]] - centerDepth) < abs(sDepths[ids[2]] - centerDepth) ? 1 : 2;
    const uint bestVertical = abs(sDepths[ids[3]] - centerDepth) < abs(sDepths[ids[4]] - centerDepth) ? 3 : 4;
    
    float3 p1, p2;
    if (bestHorizontal + bestVertical == 5) {
        p1 = float3(sPositions[ids[bestHorizontal]], sDepths[ids[bestHorizontal]]);
        p2 = float3(sPositions[ids[bestVertical]], sDepths[ids[bestVertical]]);
    }
    else {
        p1 = float3(sPositions[ids[bestVertical]], sDepths[ids[bestVertical]]);
        p2 = float3(sPositions[ids[bestHorizontal]], sDepths[ids[bestHorizontal]]);
    }
    
    const float3 centerPosition = float3(sPositions[ids[0]], centerDepth);
    const float3 normal = normalize(cross(p2 - centerPosition, p1 - centerPosition));
    const float2 uv = (float2(dispatchThreadID) + 0.5f) * ssaoSizeInverse;
    float2 noiseUv = ssaoSize * noiseSizeInverse * uv;
    float3 noise = resources.noise.SampleLevel(samplers.noiseSampler, noiseUv, 0).rgb;

    const float3 tangent = normalize(noise - normal * dot(noise, normal));
    const float3 bitangent = cross(normal, tangent);
    const float3x3 TBN = float3x3(tangent, bitangent, normal);

    float ao = 0.0f;
    for (uint i = 0; i < resources.settings.samples; i++) {
        const float3 sampleLocal = resources.samples[i].xyz;
        const float3 samplePoint = mul(sampleLocal, TBN) * resources.settings.radius + centerPosition;
        
        float4 proj = mul(float4(samplePoint, 1.0f), resources.view.camera.projection);
        proj.xyz /= proj.w;
        proj.xy = proj.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    
        if (isSaturated(proj.xy)) {
            const float depthReal = proj.z;
            const float depthSample = resources.depth.SampleLevel(samplers.depthSampler, proj.xy, 0).r;
            float depthFix = smoothstep(0.0f, 1.0f,
                resources.settings.radius / abs(proj.w - resources.view.camera.linearizeReverseZ(depthSample)));
            ao += float(depthSample >= depthReal) * depthFix;
        }
    }
    ao /= max(1.0f, float(resources.settings.samples));
    ao = pow(clamp(1.0f - ao, 0.0f, 1.0f), resources.settings.power);

    resources.ssao[dispatchThreadID] = ao;
}