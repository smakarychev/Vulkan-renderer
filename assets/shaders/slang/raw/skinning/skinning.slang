module skinning;

import "core/attributes";
import "core/geometry";
import "core/lib";
import "core/ugb";

static const uint SKIN_VERTEX_DISPATCH_COUNT = 128;

[StandaloneType("skinDispatchInfo")]
public struct SkinDispatchInfo {
    public uint renderObjectIndex;
    public uint sourceMeshletBoundIndex;
    public uint sourcePosition;
    public uint sourceNormal;
    public uint sourceTangent;
    public uint sourceJoint;
    public uint sourceWeight;
    public uint jointMatrixOffset;
    public uint skinnedVertexIndex;
    public uint skinnedMeshletBoundIndex;
    public uint vertexCount;
}

[StandaloneType("renderObjectSkinnedInfo")]
public struct RenderObjectSkinnedInfo {
    public uint renderObjectIndex;
    public uint meshletBoundIndex;
    public uint positionIndex;
    public uint normalIndex;
    public uint tangentIndex;
    public uint skinnedVertexIndex;
    public uint skinnedMeshletBoundIndex;
    public uint vertexCount;
}

[StandaloneType("skin")]
public struct Skin {
    public uint jointMatrixIndex;
    public uint jointIndex;
    public uint jointOffset;
    public uint weightIndex;
}

struct PrepareComputeSkinningResources {
    StructuredBuffer<RenderObjectSkinnedInfo> skinnedRenderObjectInfo;
    StructuredBuffer<uint> skinnedRenderObjectInfoIndices;
    RWStructuredBuffer<RenderObject> objects;
    UGBMeshlets meshlets;
    StructuredBuffer<Skin> skins;
    RWStructuredBuffer<uint> skinDispatchCount;
    RWStructuredBuffer<SkinDispatchInfo> skinDispatchesInfo;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void prepareComputeSkinning(uint dispatchThreadID: SV_DispatchThreadID,
    uniform uint skinnedRenderObjectCount,
    uniform ParameterBlock<PrepareComputeSkinningResources> resources) {

    if (dispatchThreadID >= skinnedRenderObjectCount)
        return;

    const uint skinnedInfoIndex = resources.skinnedRenderObjectInfoIndices[dispatchThreadID];
    const RenderObjectSkinnedInfo skinnedInfo = resources.skinnedRenderObjectInfo[skinnedInfoIndex];

    const uint meshletCount = resources.objects[skinnedInfo.renderObjectIndex].meshletCount;
    const uint groupMeshletCount = WaveActiveSum(meshletCount);

    uint firstMeshlet = 0;
    if (WaveIsFirstLane()) {
        InterlockedAdd(resources.skinDispatchCount[0], groupMeshletCount, firstMeshlet);
    }

    firstMeshlet = WaveReadLaneFirst(firstMeshlet);
    const uint meshletOffset = WavePrefixSum(meshletCount);

    resources.objects[skinnedInfo.renderObjectIndex].positionIndex = skinnedInfo.skinnedVertexIndex;
    resources.objects[skinnedInfo.renderObjectIndex].normalIndex = skinnedInfo.skinnedVertexIndex;
    resources.objects[skinnedInfo.renderObjectIndex].tangentIndex = skinnedInfo.skinnedVertexIndex;
    resources.objects[skinnedInfo.renderObjectIndex].meshletBoundIndex = skinnedInfo.skinnedMeshletBoundIndex;
    const RenderObject object = resources.objects[skinnedInfo.renderObjectIndex];
    const Skin skin = resources.skins[object.skinIndex];

    uint vertexOffset = 0;
    for (uint i = 0; i < meshletCount; i++) {
        const Meshlet meshlet = resources.meshlets.meshlet(object.meshletIndex + i);
        SkinDispatchInfo skinDispatchInfo;
        skinDispatchInfo.renderObjectIndex = skinnedInfo.renderObjectIndex;
        skinDispatchInfo.sourceMeshletBoundIndex = skinnedInfo.meshletBoundIndex + i;
        skinDispatchInfo.vertexCount = meshlet.vertexCount;
        
        skinDispatchInfo.sourcePosition = skinnedInfo.positionIndex + vertexOffset;
        skinDispatchInfo.sourceNormal = skinnedInfo.normalIndex + vertexOffset;
        skinDispatchInfo.sourceTangent = skinnedInfo.tangentIndex + vertexOffset;
        skinDispatchInfo.sourceJoint = skin.jointIndex + vertexOffset;
        skinDispatchInfo.sourceWeight = skin.weightIndex + vertexOffset;

        skinDispatchInfo.jointMatrixOffset = skin.jointMatrixIndex;
        skinDispatchInfo.skinnedVertexIndex = object.positionIndex + vertexOffset;
        skinDispatchInfo.skinnedMeshletBoundIndex = object.meshletBoundIndex + i;

        vertexOffset += skinDispatchInfo.vertexCount;
        resources.skinDispatchesInfo[firstMeshlet + meshletOffset + i] = skinDispatchInfo;
    }
}

public struct UGBComputeSkinning {
    RWByteAddressBuffer ugb;

    /// Read position data from ugb at a given index
    /// @param index element index of vertex position data
    public float3 position(uint index) {
        return loadAtIndex<float3>(index);
    }
    
    /// Read normal data from ugb at a given index
    /// @param index element index of vertex normal data
    public float3 normal(uint index) {
        return loadAtIndex<float3>(index);
    }
    
    /// Read tangent data from ugb at a given index
    /// @param index element index of vertex tangent data
    public float4 tangent(uint index) {
        return loadAtIndex<float4>(index);
    }

    /// Read joint data from ugb at a given index
    /// @param index element index of vertex joint data
    public uint16_t4 joint(uint index) {
        return loadAtIndex<uint16_t4>(index);
    }

    /// Read weight data from ugb at a given index
    /// @param index element index of vertex weight data
    public float4 weight(uint index) {
        return loadAtIndex<float4>(index);
    }

    /// Write skinnedVertex data to ugb at a given index
    /// @param index element index of skinnedVertex
    /// @param vertex SkinnedVertex value
    public void writeSkinnedVertex(uint index, const SkinnedVertex vertex) {
        return ugb.Store(index * sizeof(SkinnedVertex), vertex);
    }

    private ValueT loadAtIndex<ValueT>(uint index) {
        return ugb.LoadAligned<ValueT>(index * sizeof(ValueT), sizeof(ValueT));
    }
}

public struct UGBMeshletsSkinning {
    RWByteAddressBuffer meshletsUgb;

    /// Read meshlet bounds data from ugb at a given index
    /// @param index element index of meshlet bounds data
    public MeshletBounds bounds(uint index) {
        return loadAtIndex<MeshletBounds>(index);
    }

    /// Write meshlet bounds data to ugb at a given index
    /// @param bounds element index of meshlet bounds data
    public void writeBounds(uint index, MeshletBounds bounds) {
        return meshletsUgb.Store(index * sizeof(MeshletBounds), bounds);
    }
    
    /// Read meshlets data from ugb at a given index
    /// @param index element index of meshlet data
    public Meshlet meshlet(uint index) {
        return loadAtIndex<Meshlet>(index);
    }

    private ValueT loadAtIndex<ValueT>(uint index) {
        return meshletsUgb.LoadAligned<ValueT>(index * sizeof(ValueT), sizeof(ValueT));
    }
}

struct ComputeSkinningResources {
    StructuredBuffer<uint> skinDispatchCount;
    StructuredBuffer<SkinDispatchInfo> skinDispatchesInfo;
    UGBComputeSkinning ugb;
    UGBMeshletsSkinning meshlets;

    StructuredBuffer<float4x4> jointMatrices;
}

struct AABB {
    float3 min;
    float3 max;
};

groupshared AABB sAabbs[SKIN_VERTEX_DISPATCH_COUNT / 32];

[shader("compute")]
[numthreads(SKIN_VERTEX_DISPATCH_COUNT, 1, 1)]
void computeSkinning(uint workgroupID: SV_GroupID,
    uint groupThreadId: SV_GroupThreadID,
    uniform ParameterBlock<ComputeSkinningResources> resources) {

    if (workgroupID >= resources.skinDispatchCount[0])
        return;

    uint waveIndex;
    if (WaveGetLaneCount() == 64)
        waveIndex = groupThreadId >> 6;
    else if (WaveGetLaneCount() == 32)
        waveIndex = groupThreadId >> 5;

    const SkinDispatchInfo skinDispatchInfo = resources.skinDispatchesInfo[workgroupID];

    if (groupThreadId >= skinDispatchInfo.vertexCount) {
        if (WaveIsFirstLane())
            sAabbs[waveIndex] = AABB(float3(1e+11), float3(-1e+11));
        return;
    }
  
    float4x4 skinMatrix = float4x4(0.0f);
    const float4 weights = resources.ugb.weight(skinDispatchInfo.sourceWeight + groupThreadId);
    const uint16_t4 joints = resources.ugb.joint(skinDispatchInfo.sourceJoint + groupThreadId) + 
        skinDispatchInfo.jointMatrixOffset;

    skinMatrix +=
        weights.x * resources.jointMatrices[joints.x] +
        weights.y * resources.jointMatrices[joints.y] +
        weights.z * resources.jointMatrices[joints.z] +
        weights.w * resources.jointMatrices[joints.w];

    float3 position = resources.ugb.position(skinDispatchInfo.sourcePosition + groupThreadId);
    float3 normal = resources.ugb.normal(skinDispatchInfo.sourceNormal + groupThreadId);
    float4 tangent = resources.ugb.tangent(skinDispatchInfo.sourceTangent + groupThreadId);

    position = mul(float4(position, 1.0f), skinMatrix).xyz;
    normal = mul(normal, float3x3(skinMatrix));
    tangent.xyz = mul(tangent.xyz, float3x3(skinMatrix));

    resources.ugb.writeSkinnedVertex(
        skinDispatchInfo.skinnedVertexIndex + groupThreadId, SkinnedVertex(position, normal, tangent));

    const float3 minPosition = WaveActiveMin(position);
    const float3 maxPosition = WaveActiveMax(position);

    if (WaveIsFirstLane())
       sAabbs[waveIndex] = AABB(minPosition, maxPosition);

    GroupMemoryBarrierWithGroupSync();
    
    if (groupThreadId != 0)
        return;

    float3 globalMinPosition = float3(1e+11);
    float3 globalMaxPosition = float3(-1e+11);
    if (WaveGetLaneCount() == 64) {
        [unroll]
        for (uint i = 0; i < SKIN_VERTEX_DISPATCH_COUNT / 64; i++) {
            globalMinPosition = min(globalMinPosition, sAabbs[i].min);
            globalMaxPosition = max(globalMaxPosition, sAabbs[i].max);
        }
    } else if (WaveGetLaneCount() == 32) {
        [unroll]
        for (uint i = 0; i < SKIN_VERTEX_DISPATCH_COUNT / 32; i++) {
            globalMinPosition = min(globalMinPosition, sAabbs[i].min);
            globalMaxPosition = max(globalMaxPosition, sAabbs[i].max);
        }
    }

    const float3 origin = (globalMinPosition + globalMaxPosition) * 0.5f;
    const float radius = distance(globalMinPosition, globalMaxPosition) * 0.5f;

    MeshletBounds skinnedBounds = resources.meshlets.bounds(skinDispatchInfo.sourceMeshletBoundIndex);
    skinnedBounds.x = origin.x;
    skinnedBounds.y = origin.y;
    skinnedBounds.z = origin.z;
    skinnedBounds.r = radius;
    // todo: update cone

    resources.meshlets.writeBounds(skinDispatchInfo.skinnedMeshletBoundIndex, skinnedBounds);
}

struct UpdateBoundsResources {
    StructuredBuffer<RenderObjectSkinnedInfo> skinnedRenderObjectInfo;
    StructuredBuffer<uint> skinnedRenderObjectInfoIndices;
    RWStructuredBuffer<RenderObject> objects;
    UGBMeshletsSkinning meshlets;
}

[shader("compute")]
[numthreads(SKIN_VERTEX_DISPATCH_COUNT, 1, 1)]
void updateBounds(uint workgroupID: SV_GroupID,
    uint groupThreadId: SV_GroupThreadID,
    uniform uint skinnedRenderObjectCount,
    uniform ParameterBlock<UpdateBoundsResources> resources) {

    if (workgroupID >= skinnedRenderObjectCount)
        return;

    uint waveIndex;
    if (WaveGetLaneCount() == 64)
        waveIndex = groupThreadId >> 6;
    else if (WaveGetLaneCount() == 32)
        waveIndex = groupThreadId >> 5;

        
    const uint skinnedInfoIndex = resources.skinnedRenderObjectInfoIndices[workgroupID];
    const RenderObjectSkinnedInfo skinnedInfo = resources.skinnedRenderObjectInfo[skinnedInfoIndex];
    const RenderObject object = resources.objects[skinnedInfo.renderObjectIndex];
    if (groupThreadId >= object.meshletCount) {
        if (WaveIsFirstLane())
            sAabbs[waveIndex] = AABB(float3(1e+11), float3(-1e+11));
        return;
    }

    const float4x4 inverseTransform = inverse(object.transform);

    float3 groupMin = float3(1e+11);
    float3 groupMax = float3(-1e+11);
    for (uint i = groupThreadId; i < object.meshletCount; i += SKIN_VERTEX_DISPATCH_COUNT) {
        MeshletBounds bounds = resources.meshlets.bounds(object.meshletBoundIndex + i);

        const float3 minPosition = float3(bounds.x, bounds.y, bounds.z) - bounds.r;
        const float3 maxPosition = float3(bounds.x, bounds.y, bounds.z) + bounds.r;

        groupMin = min(groupMin, WaveActiveMin(minPosition));
        groupMax = max(groupMax, WaveActiveMax(maxPosition));

        const float4 originLocal = mul(float4(bounds.x, bounds.y, bounds.z, 1.0f), inverseTransform);
        const float radiusLocal = bounds.r * extractScale(inverseTransform);
        bounds.x = originLocal.x;
        bounds.y = originLocal.y;
        bounds.z = originLocal.z;
        bounds.r = radiusLocal;
        resources.meshlets.writeBounds(object.meshletBoundIndex + i, bounds);
    }

    if (WaveIsFirstLane())
        sAabbs[waveIndex] = AABB(groupMin, groupMax);

    GroupMemoryBarrierWithGroupSync();

    if (groupThreadId != 0)
        return;

    float3 globalMinPosition = float3(1e+11);
    float3 globalMaxPosition = float3(-1e+11);
    if (WaveGetLaneCount() == 64) {
        [unroll]
        for (uint i = 0; i < SKIN_VERTEX_DISPATCH_COUNT / 64; i++) {
            globalMinPosition = min(globalMinPosition, sAabbs[i].min);
            globalMaxPosition = max(globalMaxPosition, sAabbs[i].max);
        }
    } else if (WaveGetLaneCount() == 32) {
        [unroll]
        for (uint i = 0; i < SKIN_VERTEX_DISPATCH_COUNT / 32; i++) {
            globalMinPosition = min(globalMinPosition, sAabbs[i].min);
            globalMaxPosition = max(globalMaxPosition, sAabbs[i].max);
        }
    }

    const float3 origin = (globalMinPosition + globalMaxPosition) * 0.5f;
    const float radius = distance(globalMinPosition, globalMaxPosition) * 0.5f;
    const float3 originLocal = mul(float4(origin, 1.0f), inverseTransform).xyz;
    const float radiusLocal = radius * extractScale(inverseTransform);

    resources.objects[skinnedInfo.renderObjectIndex].boundingSphere = float4(originLocal, radiusLocal);
}