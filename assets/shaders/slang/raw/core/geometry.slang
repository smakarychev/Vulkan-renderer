module geometry;

import "attributes";

public static const uint BUCKETS_PER_SET = 64;

[StandaloneType("renderObject")]
public struct RenderObject {
    public float4x4 transform;
    public float4 boundingSphere;
    public uint materialId;
    public uint positionIndex;
    public uint normalIndex;
    public uint tangentIndex;
    public uint uvIndex;
}

[StandaloneType("meshlet")]
public struct Meshlet {
    public int8_t coneX;
    public int8_t coneY;
    public int8_t coneZ;
    public int8_t coneCutoff;
    public float x;
    public float y;
    public float z;
    public float r;
}

[StandaloneType("drawInfo")]
public struct DrawInfo {
    public uint count;
};

[StandaloneType("meshletBucketInfo")]
public struct MeshletBucketInfo {
    public uint index;
    public uint64_t buckets;
};

[StandaloneType("indirectCommand")]
public struct IndirectCommand {
    public uint indexCount;
    public uint instanceCount;
    public uint firstIndex;
    public int  vertexOffset;
    public uint firstInstance;
    public uint renderObject;
}

public struct IndirectDispatchCommand {
    public uint x;
    public uint y;
    public uint z;
};
