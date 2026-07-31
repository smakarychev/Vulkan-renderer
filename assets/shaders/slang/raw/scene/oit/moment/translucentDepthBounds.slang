module momentBasedOITDepthBounds;

// https://momentsingraphics.de/I3D2018.html

import "core/attributes";
import "core/lib";
import "core/geometry";
import "core/materials/material";
import "core/texturesHeap";
import "core/viewInfo";
import "core/ugb";
import "scene/visibility/visibility";

static const uint BOUNDS_DISPATCHES = 128;

public struct DepthRange {
    public uint min;
    public uint max;
}

struct DispatchResources {
    StructuredBuffer<DrawInfo> drawInfo;
    RWStructuredBuffer<IndirectDispatchCommand> command;
}

struct BoundsResources {
    ConstantBuffer<ViewInfo> view;
    UGB ugb;
    UGBMeshlets meshlets;
    StructuredBuffer<RenderObject> renderObjects;
    StructuredBuffer<SceneDrawElement> visibleMeshlets;
    StructuredBuffer<DrawInfo> drawInfos;
    RWStructuredBuffer<DepthRange> depthRange;
}



[shader("compute")]
[numthreads(1, 1, 1)]
void prepareDispatch(uniform ParameterBlock<DispatchResources> resources) {
    resources.command[0] = createIndirectDispatches(
        uint3(resources.drawInfo[0].count, 1, 1), uint3(BOUNDS_DISPATCHES, 1, 1));
}

groupshared float2 sMinMax[BOUNDS_DISPATCHES / 32];

[shader("compute")]
[numthreads(BOUNDS_DISPATCHES, 1, 1)]
void computeBounds(uint dispatchThreadID: SV_DispatchThreadID,
    uint groupThreadId: SV_GroupThreadID,
    uniform ParameterBlock<BoundsResources> resources) {

    uint waveIndex;
    if (WaveGetLaneCount() == 64)
        waveIndex = groupThreadId >> 6;
    else if (WaveGetLaneCount() == 32)
        waveIndex = groupThreadId >> 5;

    if (dispatchThreadID >= resources.drawInfos[0].count) {
        if (WaveIsFirstLane())
            sMinMax[waveIndex] = float2(1e+11, -1e+11);
        return;
    }

    const SceneDrawElement element = resources.visibleMeshlets[dispatchThreadID];
    const RenderObject renderObject = resources.renderObjects[element.renderObjectId];
    const MeshletBounds bounds = resources.meshlets.bounds(renderObject.meshletBoundIndex + element.meshletId);
    float4x4 transform = mul(renderObject.transform, resources.view.camera.view);

    const float3 origin = mul(float4(
        bounds.x,
        bounds.y,
        bounds.z, 1.0f), transform).xyz;
    const float scale = extractScale(transform);
    const float radius = bounds.r * scale;

    const float minZ = max(-origin.z - radius, resources.view.camera.near);
    const float maxZ = max(-origin.z, resources.view.camera.near) + radius;

    const float groupMin = WaveActiveMin(minZ);
    const float groupMax = WaveActiveMax(maxZ);

    sMinMax[waveIndex] = float2(groupMin, groupMax);

    GroupMemoryBarrierWithGroupSync();

    if (groupThreadId != 0)
        return;

    float globalMinZ = 1e+11;
    float globalMaxZ = -1e+11;
    if (WaveGetLaneCount() == 64) {
        [unroll]
        for (uint i = 0; i < BOUNDS_DISPATCHES / 64; i++) {
            globalMinZ = min(globalMinZ, sMinMax[i].x);
            globalMaxZ = max(globalMaxZ, sMinMax[i].y);
        }
    } else if (WaveGetLaneCount() == 32) {
        [unroll]
        for (uint i = 0; i < BOUNDS_DISPATCHES / 32; i++) {
            globalMinZ = min(globalMinZ, sMinMax[i].x);
            globalMaxZ = max(globalMaxZ, sMinMax[i].y);
        }
    }

    InterlockedMin(resources.depthRange[0].min, asuint(globalMinZ));
    InterlockedMax(resources.depthRange[0].max, asuint(globalMaxZ));
}