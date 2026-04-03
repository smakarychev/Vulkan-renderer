module renderObjectVisibility;

import "core/attributes";
import "core/geometry";
import "core/viewInfo";

import "visibility";

struct RenderObjectVisibilitySamplers {
    [ImmutableSampler(SamplerFlags.ReductionMin)]
    SamplerState samplerHiz;
}

struct RenderObjectVisibilityResources {
    Texture2D hiz[MAX_VISIBILITY_VIEWS];
    Texture2D hizPrevious[MAX_VISIBILITY_VIEWS];
    ConstantBuffer<ViewInfo> views[MAX_VISIBILITY_VIEWS];
    StructuredBuffer<RenderObject> objects;
    StructuredBuffer<uint32_t> objectHandles;

    RWStructuredBuffer<SceneVisibilityElement> visibleRenderObjects;
    RWStructuredBuffer<SceneVisibilityElement> occludedRenderObjects;
    RWStructuredBuffer<SceneVisibilityCountData> visibilityCountData;
}

[SpecializationConstant] const bool REOCCLUSION = false;
[SpecializationConstant] const bool SINGLE_PASS = false;

ParameterBlock<RenderObjectVisibilitySamplers> samplers;
ParameterBlock<RenderObjectVisibilityResources> resources;

void visibilityFirstStage(
    uint renderObjectId,
    uniform uint viewCount) {

    const RenderObject object = resources.objects[renderObjectId];

    const float4 originWorld = mul(float4(
        object.boundingSphere.x,
        object.boundingSphere.y,
        object.boundingSphere.z, 1.0f), object.transform);
    const float radius = object.boundingSphere.w * extractScale(object.transform);

    const float4 originWorldPrevious = mul(float4(
        object.boundingSphere.x,
        object.boundingSphere.y,
        object.boundingSphere.z, 1.0f), object.previousTransform);
    const float radiusPrevious = object.boundingSphere.w * extractScale(object.previousTransform);

    static_assert(MAX_VISIBILITY_VIEWS == 32, "It is required that MAX_VISIBILITY_VIEWS fits into 32 bits");
    uint visibleMask = 0;
    uint occludedMask = 0;

    for (uint i = 0; i < viewCount; i++) {
        const ViewInfo view = resources.views[i];

        float3 originView = float3(mul(originWorld, view.camera.view).xyz);
        float3 originViewPrevious = float3(mul(originWorld, view.previousCamera.view).xyz);
        if (view.camera.isDepthClamped()) {
            originView.z = min(originView.z, -view.camera.frustumNear);
            originViewPrevious.z = min(originViewPrevious.z, -view.camera.frustumNear);
        }

        const VisibilityResult visibility = isVisibleOcclusionFirstStage(
            view, samplers.samplerHiz, resources.hizPrevious[i], originView, radius, originViewPrevious, radiusPrevious);

        if (visibility == VisibilityResult.Visible)
            visibleMask |= 1 << i;
        else if (visibility == VisibilityResult.MaybeVisible) 
            occludedMask |= 1 << i;
    }

    compact<true>(
        renderObjectId,
        visibleMask, occludedMask,
        resources.visibleRenderObjects,
        resources.occludedRenderObjects,
        resources.visibilityCountData);
}


void visibilitySecondStage(
    uint renderObjectId,
    uint occludedMask,
    uniform uint viewCount) {

    const RenderObject object = resources.objects[renderObjectId];

    const float4 originWorld = mul(float4(
        object.boundingSphere.x,
        object.boundingSphere.y,
        object.boundingSphere.z, 1.0f), object.transform);
    const float radius = object.boundingSphere.w * extractScale(object.transform);

    static_assert(MAX_VISIBILITY_VIEWS == 32, "It is required that MAX_VISIBILITY_VIEWS fits into 32 bits");
    uint visibleMask = 0;

    for (uint i = 0; i < viewCount; i++) {
        if ((occludedMask & (1u << i)) == 0)
            continue;

        const ViewInfo view = resources.views[i];

        float3 originView = float3(mul(originWorld, view.camera.view).xyz);
        if (view.camera.isDepthClamped())
            originView.z = min(originView.z, -view.camera.frustumNear);

        const VisibilityResult visibility = isVisibleOcclusionSecondStage(
            view, samplers.samplerHiz, resources.hiz[i], originView, radius);

        if (visibility == VisibilityResult.Visible)
            visibleMask |= 1 << i;
    }
    
    compact<false>(
        renderObjectId,
        visibleMask, 0,
        resources.visibleRenderObjects,
        resources.occludedRenderObjects,
        resources.visibilityCountData);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void renderObjectVisibility(
    uint dispatchThreadID: SV_DispatchThreadID,
    uniform uint renderObjectCount,
    uniform uint viewCount) {

    uint maxRenderObjectIndex;
    if (REOCCLUSION)
        maxRenderObjectIndex = resources.visibilityCountData[0].occludedRenderObjects;
    else
        maxRenderObjectIndex = renderObjectCount;

    if (dispatchThreadID >= maxRenderObjectIndex)
        return;

    const uint objectHandle = resources.objectHandles[dispatchThreadID];

    if (REOCCLUSION) {
        const SceneVisibilityElement objectVisibility = resources.occludedRenderObjects[dispatchThreadID];
        visibilitySecondStage(objectVisibility.renderObjectId, objectVisibility.viewMask, viewCount);
    }
    else {
        visibilityFirstStage(objectHandle, viewCount);
    }
}

inline void compact<let isFirstStage : bool>(
    uint renderObjectId,
    uint visibleMask, uint occludedMask,
    RWStructuredBuffer<SceneVisibilityElement> visibleRenderObjects,
    RWStructuredBuffer<SceneVisibilityElement> occludedRenderObjects,
    RWStructuredBuffer<SceneVisibilityCountData> visibilityCountData) {

    const bool anyVisible = visibleMask != 0llu;
    const bool anyOccluded = occludedMask != 0llu;

    const uint totalVisibleCount = WaveActiveCountBits(anyVisible);
    uint compactedBufferIndexBase;
    uint compactedOccludedBufferIndexBase;

    if (isFirstStage) {
        const uint totalOccludedCount = WaveActiveCountBits(anyOccluded);
        if (WaveIsFirstLane()) {
            InterlockedAdd(visibilityCountData[0].visibleRenderObjects, totalVisibleCount, compactedBufferIndexBase);
            InterlockedAdd(visibilityCountData[0].occludedRenderObjects, totalOccludedCount, 
                compactedOccludedBufferIndexBase);
        }
    } else {
        if (WaveIsFirstLane()) {
            InterlockedAdd(visibilityCountData[0].visibleRenderObjectsSecondStage, totalVisibleCount,
                compactedBufferIndexBase);
            compactedBufferIndexBase += visibilityCountData[0].visibleRenderObjects;
        }
    }

    compactedBufferIndexBase = WaveReadLaneFirst(compactedBufferIndexBase);
    compactedOccludedBufferIndexBase = WaveReadLaneFirst(compactedOccludedBufferIndexBase);
    
    const uint compactedBufferOffset = WavePrefixCountBits(anyVisible);
    const uint compactedBufferIndex = compactedBufferIndexBase + compactedBufferOffset;

    if (anyVisible) {
        visibleRenderObjects[compactedBufferIndex] = SceneVisibilityElement(renderObjectId, ~0u, visibleMask);
    }

    if (isFirstStage) {
        compactedOccludedBufferIndexBase = WaveReadLaneFirst(compactedOccludedBufferIndexBase);

        const uint compactedBufferOffset = WavePrefixCountBits(anyOccluded);
        const uint compactedBufferIndex = compactedOccludedBufferIndexBase + compactedBufferOffset;

        if (anyOccluded) {
            occludedRenderObjects[compactedBufferIndex] = SceneVisibilityElement(renderObjectId, ~0u, occludedMask);
        }
    }
}