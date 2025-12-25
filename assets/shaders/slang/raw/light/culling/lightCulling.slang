module lightCulling;

__include "lightTilesBin";

import "core/viewInfo";

public static const uint LIGHT_CLUSTER_BINS_X = 60;
public static const uint LIGHT_CLUSTER_BINS_Y = 32;
public static const uint LIGHT_CLUSTER_BINS_Z = 18;

public static const uint LIGHT_TILE_SIZE_X = 16;
public static const uint LIGHT_TILE_SIZE_Y = 16;
public static const uint LIGHT_TILE_BINS_Z = 8096;
public static const uint BIN_LIGHTS_TILES_WORKGROUP = 8;
public static const uint2 BIN_LIGHTS_TILES_PER_WORKGROUP = 
    uint2(LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y) / BIN_LIGHTS_TILES_WORKGROUP;
    
public static const uint BIN_LIGHTS_TILES_PER_WORKGROUP_SQ = 
    LIGHT_TILE_SIZE_X / BIN_LIGHTS_TILES_WORKGROUP * 
    LIGHT_TILE_SIZE_Y / BIN_LIGHTS_TILES_WORKGROUP;

public static const uint BIN_DISPATCH_SIZE = 256;

namespace lightCulling {
internal uint sliceIndex(float depth, float n, float f, uint count) {
    const float z = (n - f) * depth - n;
    const float logInv = 1.0f / log(f / n);

    return uint(floor(log(f) * count * logInv - log(-z) * count * logInv));
}

public uint sliceIndexLinearDepth(float z, float n, float f) {
    const float logInv = 1.0f / log(f / n);

    return LIGHT_CLUSTER_BINS_Z -
        uint(floor(log(f) * LIGHT_CLUSTER_BINS_Z * logInv - log(-z) * LIGHT_CLUSTER_BINS_Z * logInv)) - 1;
}

public uint clusterIndex(float2 uv, uint slice) {
    const uint3 indices =
        uint3(uint2(float2(uv.x, 1.0f - uv.y) * float2(LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y)), slice);

    return indices.x +
        indices.y * LIGHT_CLUSTER_BINS_X +
        indices.z * LIGHT_CLUSTER_BINS_X * LIGHT_CLUSTER_BINS_Y;
}

public uint getZBinIndex(float depth, float near, float far) {
    const float z = -Camera.linearizeReverseZ(depth, near, far);
    const float depthRange = far - near;
    
    return uint(z / depthRange * LIGHT_TILE_BINS_Z);
}

public uint getTileIndex(float2 uv, float2 resolution, uint2 lightCullTileCount) {
    const uint2 tileIndex = uint2(uv * resolution) / uint2(LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y);

    return flatten2d(tileIndex, lightCullTileCount);
}

public float3 linePlaneIntersection(float3 dir, float z) {
    return float3(dir.xy * z / dir.z, z);
}

public float distanceAabbSquared(float3 point, float4 min, float4 max) {
    float distance2 = 0.0f;
    for (uint i = 0; i < 3; i++) {
        const float c = point[i];
        if (c < min[i])
            distance2 += (min[i] - c) * (min[i] - c);
        if (c > max[i])
            distance2 += (max[i] - c) * (max[i] - c);
    }

    return distance2;
}
}

public static const uint VIEW_MAX_LIGHTS = 1024;
public static const uint BIN_BIT_SIZE = 32;
public static const uint BIN_COUNT = VIEW_MAX_LIGHTS / BIN_BIT_SIZE;

public struct LightCluster {
    internal float4 min;
    internal float4 max;
    public uint bins[BIN_COUNT];
};

public struct Plane {
    /// xyz components for normal, w for offset
    float4 plane;

    /// Constructs plane by three points
    internal __init(float3 a, float3 b, float3 c) {
        const float3 v0 = b - a;
        const float3 v1 = c - a;
        
        plane.xyz = normalize(cross(v0, v1));
        // plane.w is always zero here because it is a subfrustum plane in view space
        plane.w = 0;
    }
};

public struct LightTile {
    /// tile is 4 planes, near and far are computed separately
    internal Plane planes[4];
    public uint bins[BIN_COUNT];
};

public struct LightZBin {
    public uint16_t min;
    public uint16_t max;
};