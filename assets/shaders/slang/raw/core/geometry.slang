module geometry;

import "attributes";

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
