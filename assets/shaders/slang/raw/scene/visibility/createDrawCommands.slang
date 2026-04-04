module createDrawCommands;


import "core/geometry";
import "core/ugb";

import "visibility";

[SpecializationConstant] const bool REOCCLUSION = false;
static const uint MAX_COMMAND_BUFFERS = BUCKETS_PER_SET * 2;
static const uint CREATE_COMMANDS_DISPATCHES = 256;

struct CreateIndirectDispatchesResources {
    StructuredBuffer<SceneVisibilityCountData> visibilityCountData;
    RWStructuredBuffer<IndirectDispatchCommand> command;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void createDrawCommandsDispatches(
    uint dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<CreateIndirectDispatchesResources> resources) {

    uint visibleCount = 0;
    if (REOCCLUSION)
        visibleCount = resources.visibilityCountData[0].visibleMeshletsSecondStage;
    else
        visibleCount = resources.visibilityCountData[0].visibleMeshlets;
    visibleCount = max(1, visibleCount);

    resources.command[0] = createIndirectDispatches(
        uint3(visibleCount, 1, 1), uint3(CREATE_COMMANDS_DISPATCHES, 1, 1));
}

struct Resources {
    UGBMeshlets meshlets;
    StructuredBuffer<RenderObject> objects;
    StructuredBuffer<uint32_t> objectHandles;
    StructuredBuffer<uint64_t> renderObjectBuckets;
    RWStructuredBuffer<SceneVisibilityElement> visibleMeshlets;
    StructuredBuffer<SceneVisibilityCountData> visibilityCountData;
    RWStructuredBuffer<IndirectCommand> drawCommands[MAX_COMMAND_BUFFERS];
    RWStructuredBuffer<DrawInfo> drawInfos[MAX_COMMAND_BUFFERS];
}


[shader("compute")]
[numthreads(CREATE_COMMANDS_DISPATCHES, 1, 1)]
void createDrawCommands(
    uint dispatchThreadID: SV_DispatchThreadID,
    uniform uint viewCount,
    uniform uint bucketCount,
    uniform uint64_t availableBucketsMask,
    uniform ParameterBlock<Resources> resources) {

    uint visibleCount = 0;
    if (REOCCLUSION)
        visibleCount = resources.visibilityCountData[0].visibleMeshletsSecondStage;
    else
        visibleCount = resources.visibilityCountData[0].visibleMeshlets;

    if (dispatchThreadID >= visibleCount)
        return;

    uint meshletId = dispatchThreadID;
    if (REOCCLUSION)
        meshletId += resources.visibilityCountData[0].visibleMeshlets;

    const SceneVisibilityElement meshletElement = resources.visibleMeshlets[meshletId];
    const uint renderObjectId = resources.objectHandles[meshletElement.renderObjectHandle];
    const RenderObject object = resources.objects[renderObjectId];
    const uint64_t objectBuckets = resources.renderObjectBuckets[meshletElement.renderObjectHandle];
    const Meshlet meshlet = resources.meshlets.meshlet(object.meshletIndex + meshletElement.meshletId);

    uint64_t buckets = WaveActiveBitOr(objectBuckets & availableBucketsMask);
    uint views = WaveActiveBitOr(meshletElement.viewMask);
    while (views > 0) {
        int viewBit = firstbitlow(views);

        writeView(viewBit * bucketCount, views, viewBit, buckets, objectBuckets, renderObjectId, meshletId, meshlet, 
            object, resources);

        views = views & ~(1u << viewBit);
    }

    // change renderObjectHandle (which is Set-specific) to renderObjectId (which is Geometry-specific)
    resources.visibleMeshlets[meshletId].renderObjectHandle = renderObjectId;
}

void writeView(uint viewOffset, const uint views, const int viewBit, uint64_t buckets, uint64_t objectBuckets,
    uint renderObjectId, uint meshletId, const Meshlet meshlet, const RenderObject object,
    uniform ParameterBlock<Resources> resources) {

    const uint bit = 1u << viewBit;
    const bool hasView = (views & bit) != 0;

    const uint hasViewCount = WaveActiveCountBits(hasView);
    if (hasViewCount == 0)
        return;

    while (buckets > 0) {
        int bucketBit = firstbitlow(uint(buckets));
        bucketBit = bucketBit != -1 ? bucketBit : firstbitlow(uint(buckets >> 32)) + 32;
        buckets = compact(viewOffset, hasView, bucketBit, buckets, objectBuckets, renderObjectId, meshletId, meshlet, 
            object, resources);
    }
}

uint64_t compact(uint viewOffset, bool hasView, int bucketBit, uint64_t buckets, uint64_t objectBuckets,
    uint renderObjectId, uint meshletId, const Meshlet meshlet, const RenderObject object,
    uniform ParameterBlock<Resources> resources) {

    const uint64_t bit = uint64_t(1) << bucketBit;
    const bool hasBucket = (objectBuckets & bit) != 0 && hasView;
    const uint hasBucketsCount = WaveActiveCountBits(hasBucket);

    uint firstDrawIndex = 0;
    if (WaveIsFirstLane())
        InterlockedAdd(resources.drawInfos[viewOffset + bucketBit][0].count, hasBucketsCount, firstDrawIndex);
    firstDrawIndex = WaveReadLaneFirst(firstDrawIndex);

    if (hasBucket) {
        const uint drawIndexOffset = WavePrefixCountBits(hasBucket);
        resources.drawCommands[viewOffset + bucketBit][firstDrawIndex + drawIndexOffset] = IndirectCommand(
            meshlet.indexCount, 1, object.indexIndex + meshlet.firstIndex, meshlet.firstVertex,
            meshletId, renderObjectId
        );
    }

    return buckets & ~bit;
}