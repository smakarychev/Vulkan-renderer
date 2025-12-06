module shadow;

import "core/attributes";

[StandaloneType("csmData")]
public struct CsmData {
    public static const uint MAX_CASCADES = 5;

    public uint cascadeCount;
    public float cascades[MAX_CASCADES];
    public float4x4 viewProjections[MAX_CASCADES];
    public float4x4 views[MAX_CASCADES];
    public float near[MAX_CASCADES];
    public float far[MAX_CASCADES];
}
