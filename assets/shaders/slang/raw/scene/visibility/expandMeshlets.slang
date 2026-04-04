module expandMeshlets;

import "core/geometry";

import "visibility";

struct CreateIndirectDispatchesResources {
    StructuredBuffer<SceneVisibilityCountData> visibilityCountData;
    RWStructuredBuffer<IndirectDispatchCommand> command;
}

[SpecializationConstant] const bool REOCCLUSION = false;
static const uint EXPAND_MESHLETS_DISPATCHES = 1024;

[shader("compute")]
[numthreads(1, 1, 1)]
void createIndirectMeshletDispatches(
    uint dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<CreateIndirectDispatchesResources> resources) {

    uint visibleCount = 0;
    if (REOCCLUSION)
        visibleCount = resources.visibilityCountData[0].visibleRenderObjectsSecondStage;
    else
        visibleCount = resources.visibilityCountData[0].visibleRenderObjects;
    visibleCount = max(1, visibleCount);

    resources.command[0] = createIndirectDispatches(
        uint3(visibleCount, 1, 1), uint3(1, 1, 1));
}

struct ExpandMeshletsResources {
    StructuredBuffer<RenderObject> renderObjects;
    StructuredBuffer<uint32_t> objectHandles;
    StructuredBuffer<SceneVisibilityElement> visibleRenderObjects;
    RWStructuredBuffer<SceneVisibilityCountData> visibilityCountData;
    RWStructuredBuffer<SceneVisibilityElement> expandedMeshlets;
}

groupshared uint sMeshletOffset;

[shader("compute")]
[numthreads(EXPAND_MESHLETS_DISPATCHES, 1, 1)]
void expandMeshlets(
    uint workgroupID: SV_GroupID,
    uint groupThreadId: SV_GroupThreadID,
    uniform ParameterBlock<ExpandMeshletsResources> resources) {

    uint visibleRenderObjectsCount = 0;
    if (REOCCLUSION)
        visibleRenderObjectsCount = resources.visibilityCountData[0].visibleRenderObjectsSecondStage;
    else
        visibleRenderObjectsCount = resources.visibilityCountData[0].visibleRenderObjects;

    if (workgroupID >= visibleRenderObjectsCount)
        return;

    uint renderObjectIndex = workgroupID;
    if (REOCCLUSION)
        renderObjectIndex += resources.visibilityCountData[0].visibleRenderObjects;

    const SceneVisibilityElement renderObjectElement = resources.visibleRenderObjects[renderObjectIndex];
    const uint renderObjectId = resources.objectHandles[renderObjectElement.renderObjectHandle];
    const RenderObject renderObject = resources.renderObjects[renderObjectId];

    if (groupThreadId == 0) {
         if (REOCCLUSION)
            InterlockedAdd(resources.visibilityCountData[0].expandedMeshletsCountSecondStage, renderObject.meshletCount,
                sMeshletOffset);
        else
            InterlockedAdd(resources.visibilityCountData[0].expandedMeshletsCount, renderObject.meshletCount,
                sMeshletOffset);
    }
    GroupMemoryBarrierWithGroupSync();

    for (uint i = groupThreadId; i < renderObject.meshletCount; i += EXPAND_MESHLETS_DISPATCHES)
        resources.expandedMeshlets[sMeshletOffset + i] = 
            SceneVisibilityElement(renderObjectElement.renderObjectHandle, i, renderObjectElement.viewMask);
}