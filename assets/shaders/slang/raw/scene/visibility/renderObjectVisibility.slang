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
    ConstantBuffer<ViewInfo> views[MAX_VISIBILITY_VIEWS];
    StructuredBuffer<RenderObject> objects;
    RWStructuredBuffer<VisibilityBucketBits> objectsVisibility[MAX_VISIBILITY_VIEWS];
}

[SpecializationConstant] const bool REOCCLUSION = false;
[SpecializationConstant] const bool SINGLE_PASS = false;

ParameterBlock<RenderObjectVisibilitySamplers> samplers;
ParameterBlock<RenderObjectVisibilityResources> resources;

[shader("compute")]
[numthreads(64, 1, 1)]
void renderObjectVisibility(
    uint dispatchThreadID: SV_DispatchThreadID,
    uniform uint renderObjectCount,
    uniform uint viewCount) {

    if (dispatchThreadID >= renderObjectCount)
        return;

    const VisibilityBucketIndex bucketIndex = VisibilityBucketIndex(dispatchThreadID);

    const RenderObject object = resources.objects[dispatchThreadID];

    const float4 originWorld = mul(float4(
        object.boundingSphere.x,
        object.boundingSphere.y,
        object.boundingSphere.z, 1.0f), object.transform);
    const float radius = object.boundingSphere.w * extractScale(object.transform);

    for (uint i = 0; i < viewCount; i++) {
        const ViewInfo view = resources.views[i];

        float3 originView = float3(mul(originWorld, view.camera.view).xyz);
        if (view.camera.isDepthClamped())
            originView.z = min(originView.z, -view.camera.frustumNear);

        const bool isLastFrameVisible = resources.objectsVisibility[i][bucketIndex.bucket].isVisible(
            bucketIndex.bitHigh,
            bucketIndex.bitLow);
        bool visible = false;

        if (SINGLE_PASS)
            visible = isReocclusionVisible(view, samplers.samplerHiz, resources.hiz[i], originView, radius);
        else if (!REOCCLUSION)
            visible = isLastFrameVisible && isVisible(view, originView, radius);
        else
            visible = isReocclusionVisible(view, samplers.samplerHiz, resources.hiz[i], originView, radius);

        const uint4 visibleBallot = WaveActiveBallot(visible);
        if (WaveIsFirstLane()) {
            if (WaveGetLaneCount() == 64) {
                resources.objectsVisibility[i][bucketIndex.bucket].visibility[0] = visibleBallot.r;
                resources.objectsVisibility[i][bucketIndex.bucket].visibility[1] = visibleBallot.g;
            } else if (WaveGetLaneCount() == 32)  {
                /*uint waveIndex =  dispatchThreadID / 32; */
                uint waveIndex = dispatchThreadID >> 5;
                resources.objectsVisibility[i][bucketIndex.bucket].visibility[waveIndex] = visibleBallot.r;  
            }
        }
    }
}