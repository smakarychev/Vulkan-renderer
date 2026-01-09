module prepareVisibileMeshlets;

import "core/geometry";

struct PrepareVisibileMeshletsResources {
    StructuredBuffer<IndirectCommand> commands;
    StructuredBuffer<uint64_t> objectBuckets;
    StructuredBuffer<uint> meshletsHandles;
    RWStructuredBuffer<MeshletBucketInfo> meshletInfos;
    RWStructuredBuffer<uint> meshletInfoCounts;
}

ParameterBlock<PrepareVisibileMeshletsResources> resources;

[shader("compute")]
[numthreads(64, 1, 1)]
void prepareVisibileMeshlets(
    uint dispatchThreadID: SV_DispatchThreadID,
    uniform uint handleCount) {

    resources.meshletInfoCounts[0] = handleCount;
    if (dispatchThreadID >= handleCount)
        return;
    const uint meshletId = resources.meshletsHandles[dispatchThreadID];
    const uint renderObjectId = resources.commands[meshletId].renderObject;

    MeshletBucketInfo info = MeshletBucketInfo(meshletId, resources.objectBuckets[renderObjectId]);
    resources.meshletInfos[dispatchThreadID] = info;
}