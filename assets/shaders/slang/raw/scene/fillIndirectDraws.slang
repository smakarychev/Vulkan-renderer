module fillIndirectDraws;

import "core/geometry";

struct Resources {
    StructuredBuffer<IndirectCommand> referenceCommands;
    StructuredBuffer<MeshletBucketInfo> meshletInfos;
    ConstantBuffer<uint> meshletInfoCounts;
    RWStructuredBuffer<IndirectCommand> drawCommands[BUCKETS_PER_SET];
    RWStructuredBuffer<DrawInfo> drawInfos[BUCKETS_PER_SET];
}

ParameterBlock<Resources> resources;

[shader("compute")]
[numthreads(256, 1, 1)]
void fillIndirectDraws(
    uint dispatchThreadID: SV_DispatchThreadID,
    uniform uint64_t availableBucketsMask) {

    if (dispatchThreadID >= resources.meshletInfoCounts)
        return;

    const MeshletBucketInfo meshlet = resources.meshletInfos[dispatchThreadID];
    uint64_t buckets = WaveActiveBitOr(meshlet.buckets) & availableBucketsMask;
    while (buckets > 0) {
        int bitIndex = firstbitlow(uint(buckets));
        bitIndex = bitIndex != -1 ? bitIndex : firstbitlow(uint(buckets >> 32)) + 32;
        buckets = compact(bitIndex, meshlet, buckets);
    }
}

uint64_t compact(const int bitIndex, const MeshletBucketInfo meshlet, const uint64_t buckets) {

    const uint64_t bit = uint64_t(1) << bitIndex;
    const bool hasBit = (meshlet.buckets & bit) != 0;

    const uint hasBitCount = WaveActiveCountBits(hasBit);

    uint firstDrawIndex = 0;
    if (WaveIsFirstLane())
        InterlockedAdd(resources.drawInfos[bitIndex][0].count, hasBitCount, firstDrawIndex);
    firstDrawIndex = WaveReadLaneFirst(firstDrawIndex);

    if (hasBit) {
        const uint drawIndexOffset = WavePrefixCountBits(hasBit);
        resources.drawCommands[bitIndex][firstDrawIndex + drawIndexOffset] =
            resources.referenceCommands[meshlet.index];
    }

    return buckets & ~bit;
}