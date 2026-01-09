module meshletVisibility;

import "core/attributes";
import "core/geometry";
import "core/viewInfo";

import "visibility";

struct MeshletVisibilitySamplers {
    [ImmutableSampler(SamplerFlags.ReductionMin)]
    SamplerState samplerHiz;
}

struct MeshletVisibilityResources {
    Texture2D hiz[MAX_VISIBILITY_VIEWS];
    ConstantBuffer<ViewInfo> views[MAX_VISIBILITY_VIEWS];
    StructuredBuffer<IndirectCommand> commands;
    StructuredBuffer<RenderObject> objects;
    StructuredBuffer<uint64_t> objectBuckets;
    StructuredBuffer<Meshlet> meshlets;
    StructuredBuffer<uint> meshletsHandles;
    StructuredBuffer<VisibilityBucketBits> objectsVisibility[MAX_VISIBILITY_VIEWS];
    RWStructuredBuffer<MeshletBucketInfo> meshletInfos[MAX_VISIBILITY_VIEWS];
    RWStructuredBuffer<uint> meshletInfoCounts[MAX_VISIBILITY_VIEWS];
    RWStructuredBuffer<VisibilityBucketBits> meshletsVisibilty[MAX_VISIBILITY_VIEWS];
}

[SpecializationConstant] const bool REOCCLUSION = false;
[SpecializationConstant] const bool SINGLE_PASS = false;

ParameterBlock<MeshletVisibilitySamplers> samplers;
ParameterBlock<MeshletVisibilityResources> resources;

[shader("compute")]
[numthreads(64, 1, 1)]
void meshletVisibility(
    uint dispatchThreadID: SV_DispatchThreadID,
    uniform uint handleCount,
    uniform uint viewCount) {

    if (dispatchThreadID >= handleCount)
        return;

    const uint meshletId = resources.meshletsHandles[dispatchThreadID];
    const uint renderObjectId = resources.commands[meshletId].renderObject;
    const Meshlet meshlet = resources.meshlets[meshletId];

    const VisibilityBucketIndex renderObjectBucketIndex = VisibilityBucketIndex(renderObjectId);
    const VisibilityBucketIndex meshletBucketIndex = VisibilityBucketIndex(meshletId);

    const float4 originWorld = mul(float4(
        meshlet.x,
        meshlet.y,
        meshlet.z, 1.0f), resources.objects[renderObjectId].transform);
    const float scale = extractScale(resources.objects[renderObjectId].transform);
    const float radius = meshlet.r * scale;

    for (uint i = 0; i < viewCount; i++) {
        const ViewInfo view = resources.views[i];

        float3 originView = mul(originWorld, view.camera.view).xyz;
        if (view.camera.isDepthClamped())
            originView.z = min(originView.z, -view.camera.frustumNear);

        const bool isRenderObjectVisible = resources.objectsVisibility[i][renderObjectBucketIndex.bucket].isVisible(
            renderObjectBucketIndex.bitHigh,
            renderObjectBucketIndex.bitLow);
        const bool isLastFrameVisible = resources.meshletsVisibilty[i][meshletBucketIndex.bucket].isVisible(
            meshletBucketIndex.bitHigh,
            meshletBucketIndex.bitLow);

        bool visible = false;

        if (SINGLE_PASS) {
            // this is for order-independent translucency rendering
            visible = isRenderObjectVisible && 
                isReocclusionVisible(view, samplers.samplerHiz, resources.hiz[i], originView, radius);
        }
        else if (!REOCCLUSION) {
            visible = isRenderObjectVisible && isLastFrameVisible && isVisible(view, originView, radius);
        }
        else {
            visible = isRenderObjectVisible && 
                isReocclusionVisible(view, samplers.samplerHiz, resources.hiz[i], originView, radius);

            const uint4 visibleBallot = WaveActiveBallot(visible);
            if (WaveIsFirstLane()) {
                if (WaveGetLaneCount() == 64) {
                    resources.meshletsVisibilty[i][meshletBucketIndex.bucket].visibility[0] = visibleBallot.r;
                    resources.meshletsVisibilty[i][meshletBucketIndex.bucket].visibility[1] = visibleBallot.g;
                } else if (WaveGetLaneCount() == 32)  {
                    /*uint waveIndex =  dispatchThreadID / 32; */
                    uint waveIndex = dispatchThreadID >> 5;
                    resources.meshletsVisibilty[i][meshletBucketIndex.bucket].visibility[waveIndex] = visibleBallot.r;  
                }
            }

            visible = visible && !isLastFrameVisible;
        }

        compact(meshletId, resources.objectBuckets[renderObjectId], visible,
            resources.meshletInfos[i], resources.meshletInfoCounts[i]);
    }
}

void compact(uint meshletId, uint64_t buckets, bool visible,
    RWStructuredBuffer<MeshletBucketInfo> meshletInfo, RWStructuredBuffer<uint> meshletInfoCount) {

    const uint totalVisibleCount = WaveActiveCountBits(visible);
    uint compactedBufferIndexBase;
    if (WaveIsFirstLane())
        InterlockedAdd(meshletInfoCount[0], totalVisibleCount, compactedBufferIndexBase);
    compactedBufferIndexBase = WaveReadLaneFirst(compactedBufferIndexBase);

    const uint compactedBufferOffset = WavePrefixCountBits(visible);
    const uint compactedBufferIndex = compactedBufferIndexBase + compactedBufferOffset;

    if (visible) {
        MeshletBucketInfo info = MeshletBucketInfo(meshletId, buckets);
        meshletInfo[compactedBufferIndex] = info;
    }
}