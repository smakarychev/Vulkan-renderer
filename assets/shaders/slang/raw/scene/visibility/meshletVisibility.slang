module meshletVisibility;

import "core/attributes";
import "core/geometry";
import "core/viewInfo";
import "core/ugb";

import "visibility";

struct MeshletVisibilitySamplers {
    [ImmutableSampler(SamplerFlags.ReductionMin)]
    SamplerState samplerHiz;
}

struct MeshletVisibilityResources {
    Texture2D hiz[MAX_VISIBILITY_VIEWS];
    Texture2D hizPrevious[MAX_VISIBILITY_VIEWS];
    ConstantBuffer<ViewInfo> views[MAX_VISIBILITY_VIEWS];
    StructuredBuffer<RenderObject> objects;
    StructuredBuffer<uint32_t> objectHandles;

    UGBMeshlets meshlets;
    
    StructuredBuffer<SceneVisibilityElement> expandedMeshlets;
    RWStructuredBuffer<SceneVisibilityElement> visibleMeshlets;
    RWStructuredBuffer<SceneVisibilityElement> occludedMeshlets;
    RWStructuredBuffer<SceneVisibilityCountData> visibilityCountData;
}

[SpecializationConstant] const bool REOCCLUSION = false;
[SpecializationConstant] const bool SINGLE_PASS = false;

ParameterBlock<MeshletVisibilitySamplers> samplers;
ParameterBlock<MeshletVisibilityResources> resources;

void visibilityFirstStage(
    uint renderObjectHandle,
    uint meshletId,
    uint viewMask,
    uniform uint viewCount) {

    const RenderObject object = resources.objects[resources.objectHandles[renderObjectHandle]];
    const MeshletBounds meshlet = resources.meshlets.bounds(object.meshletBoundsIndex + meshletId);

    const float4 originWorld = mul(float4(
        meshlet.x,
        meshlet.y,
        meshlet.z, 1.0f), object.transform);
    const float scale = extractScale(object.transform);
    const float radius = meshlet.r * scale;

    const float4 originWorldPrevious = mul(float4(
        meshlet.x,
        meshlet.y,
        meshlet.z, 1.0f), object.previousTransform);
    const float scalePrevious = extractScale(object.previousTransform);
    const float radiusPrevious = meshlet.r * scalePrevious;

    uint visibleMask = 0;
    uint occludedMask = 0;

    for (uint i = 0; i < viewCount; i++) {
        // todo: scalarize?
        if ((viewMask & (1u << i)) == 0)
            continue;

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
        renderObjectHandle,
        meshletId,
        visibleMask, occludedMask,
        resources.visibleMeshlets,
        resources.occludedMeshlets,
        resources.visibilityCountData);
}


void visibilitySecondStage(
    uint renderObjectHandle,
    uint meshletId,
    uint occludedMask,
    uniform uint viewCount) {

    const RenderObject object = resources.objects[resources.objectHandles[renderObjectHandle]];
    const MeshletBounds meshlet = resources.meshlets.bounds(object.meshletBoundsIndex + meshletId);

    const float4 originWorld = mul(float4(
        meshlet.x,
        meshlet.y,
        meshlet.z, 1.0f), object.transform);
    const float scale = extractScale(object.transform);
    const float radius = meshlet.r * scale;

    const float4 originWorldPrevious = mul(float4(
        meshlet.x,
        meshlet.y,
        meshlet.z, 1.0f), object.previousTransform);
    const float scalePrevious = extractScale(object.previousTransform);
    const float radiusPrevious = meshlet.r * scalePrevious;

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
        renderObjectHandle,
        meshletId,
        visibleMask, 0,
        resources.visibleMeshlets,
        resources.occludedMeshlets,
        resources.visibilityCountData);
}


[shader("compute")]
[numthreads(64, 1, 1)]
void meshletVisibility(
    uint dispatchThreadID: SV_DispatchThreadID,
    uniform uint viewCount) {

    uint expandedCount = 0;
    if (REOCCLUSION)
        expandedCount = resources.visibilityCountData[0].expandedMeshletsCountSecondStage;
    else
        expandedCount = resources.visibilityCountData[0].expandedMeshletsCount;
    if (dispatchThreadID >= expandedCount)
        return;

    const SceneVisibilityElement meshletVisiblityElement = resources.expandedMeshlets[dispatchThreadID];
    if (REOCCLUSION)
        visibilitySecondStage(
            meshletVisiblityElement.renderObjectHandle,
            meshletVisiblityElement.meshletId, 
            meshletVisiblityElement.viewMask, 
            viewCount);
    else
        visibilityFirstStage(
            meshletVisiblityElement.renderObjectHandle,
            meshletVisiblityElement.meshletId, 
            meshletVisiblityElement.viewMask, 
            viewCount);
}

inline void compact<let isFirstStage : bool>(
    uint renderObjectHandle,
    uint meshletId,
    uint visibleMask, uint occludedMask,
    RWStructuredBuffer<SceneVisibilityElement> visibleMeshlets,
    RWStructuredBuffer<SceneVisibilityElement> occludedMeshlets,
    RWStructuredBuffer<SceneVisibilityCountData> visibilityCountData) {

    const bool anyVisible = visibleMask != 0llu;
    const bool anyOccluded = occludedMask != 0llu;

    const uint totalVisibleCount = WaveActiveCountBits(anyVisible);
    uint compactedBufferIndexBase;
    uint compactedOccludedBufferIndexBase;

    if (isFirstStage) {
        const uint totalOccludedCount = WaveActiveCountBits(anyOccluded);
        if (WaveIsFirstLane()) {
            InterlockedAdd(visibilityCountData[0].visibleMeshlets, totalVisibleCount, compactedBufferIndexBase);
            InterlockedAdd(visibilityCountData[0].occludedMeshlets, totalOccludedCount, 
                compactedOccludedBufferIndexBase);
        }
    } else {
        if (WaveIsFirstLane()) {
            InterlockedAdd(visibilityCountData[0].visibleMeshletsSecondStage, totalVisibleCount,
                           compactedBufferIndexBase);
            compactedBufferIndexBase += visibilityCountData[0].visibleMeshlets;
        }
    }

    compactedBufferIndexBase = WaveReadLaneFirst(compactedBufferIndexBase);
    compactedOccludedBufferIndexBase = WaveReadLaneFirst(compactedOccludedBufferIndexBase);
    
    const uint compactedBufferOffset = WavePrefixCountBits(anyVisible);
    const uint compactedBufferIndex = compactedBufferIndexBase + compactedBufferOffset;

    if (anyVisible) {
        visibleMeshlets[compactedBufferIndex] = SceneVisibilityElement(renderObjectHandle, meshletId, visibleMask);
    }

    if (isFirstStage) {
        compactedOccludedBufferIndexBase = WaveReadLaneFirst(compactedOccludedBufferIndexBase);

        const uint compactedBufferOffset = WavePrefixCountBits(anyOccluded);
        const uint compactedBufferIndex = compactedOccludedBufferIndexBase + compactedBufferOffset;

        if (anyOccluded) {
            occludedMeshlets[compactedBufferIndex] = SceneVisibilityElement(renderObjectHandle, meshletId, occludedMask);
        }
    }
}