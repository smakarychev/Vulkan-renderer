module visibility;

import "core/viewInfo";

public static const uint MAX_VISIBILITY_VIEWS = 64;

public struct VisibilityBucketBits
{
    // store uint64_t as 2 uints because of subgroup operations
    public uint visibility[2];

    public bool isVisible(uint bitHigh, uint bitLow) {
        return (visibility[bitHigh] & (uint(1) << bitLow)) != 0;
    }
};

public struct VisibilityBucketIndex
{
    public uint bucket;
    public uint bitHigh;
    public uint bitLow;

    public __init(uint id) {
        static const uint BUCKET_BIT_SHIFT = 6;
        bucket = id >> BUCKET_BIT_SHIFT;
        const uint bitIndex = id & uint((1 << BUCKET_BIT_SHIFT) - 1);
        bitHigh = bitIndex >> (BUCKET_BIT_SHIFT - 1);
        bitLow = bitIndex & uint((1 << (BUCKET_BIT_SHIFT - 1)) - 1);
    }
};

public float extractScale(float4x4 matrix) {
    const float l1 = dot(matrix[0], matrix[0]);
    const float l2 = dot(matrix[1], matrix[1]);
    const float l3 = dot(matrix[2], matrix[2]);
    const float maxL = max(l1, max(l2, l3));

    return sqrt(maxL);
}

public bool isBackfaceMeshletVisible(float3 sphereOrigin, float radius, float3 coneAxis, float coneCutoff) {
    return dot(sphereOrigin, coneAxis) < coneCutoff * length(sphereOrigin) + radius;
}

public bool isBackfaceMeshletVisibleOrthographic(float3 coneAxis, float coneCutoff) {
    return -coneAxis.z < coneCutoff;
}

public bool isFrustumVisibleOrthographic(float3 sphereOrigin, float radius, const Camera camera) {
    bool visible = true;

    visible = visible && 
        abs(camera.frustumRightX * sphereOrigin.x + camera.projectionBiasX) < 1 + abs(camera.frustumRightX) * radius;
    visible = visible && 
        abs(camera.frustumTopY * sphereOrigin.y + camera.projectionBiasY) < 1 + abs(camera.frustumTopY) * radius;
    visible = visible &&
        sphereOrigin.z - radius <= -camera.frustumNear &&
        sphereOrigin.z + radius >= -camera.frustumFar;

    return visible;
}

public bool isFrustumVisible(float3 sphereOrigin, float radius, const Camera camera) {
    bool visible = true;
    visible = visible && camera.frustumRightX * abs(sphereOrigin.x) + sphereOrigin.z * camera.frustumRightZ < radius;
    visible = visible && camera.frustumTopY * abs(sphereOrigin.y) + sphereOrigin.z * camera.frustumTopZ < radius;
    visible = visible &&
        sphereOrigin.z - radius <= -camera.frustumNear &&
        sphereOrigin.z + radius >= -camera.frustumFar;

    return visible;
}

public bool isOcclusionVisible(float3 sphereOrigin, float radius, const Camera camera, SamplerState samplerHiz,
    Texture2D hiz) {
    if (sphereOrigin.z + radius >= -camera.frustumNear)
        return true;

    const float3 cr = sphereOrigin * radius;
    const float czr2 = sphereOrigin.z * sphereOrigin.z - radius * radius;

    const float vx = sqrt(sphereOrigin.x * sphereOrigin.x + czr2);
    const float minx = (vx * sphereOrigin.x - cr.z) / (vx * sphereOrigin.z + cr.x);
    const float maxx = (vx * sphereOrigin.x + cr.z) / (vx * sphereOrigin.z - cr.x);

    const float vy = sqrt(sphereOrigin.y * sphereOrigin.y + czr2);
    const float miny = (vy * sphereOrigin.y - cr.z) / (vy * sphereOrigin.z + cr.y);
    const float maxy = (vy * sphereOrigin.y + cr.z) / (vy * sphereOrigin.z - cr.y);

    float4 aabb = float4(minx, miny, maxx, maxy) *
        float4(camera.projectionWidth, camera.projectionHeight, camera.projectionWidth, camera.projectionHeight);   
    // clip space -> uv space
    aabb = aabb.xyzw * -0.5f + 0.5f;

    const float width  = (aabb.x - aabb.z) * camera.hizResolution.x;
    const float height = (aabb.w - aabb.y) * camera.hizResolution.y;

    const float level = ceil(log2(max(width, height)));

    const float depth = hiz.SampleLevel(samplerHiz, (aabb.xy + aabb.zw) * 0.5f, level).r;      
    const float projectedDepth = -camera.frustumNear / (sphereOrigin.z + radius);

    return projectedDepth >= depth;
}

public bool isOcclusionVisibleOrthographic(float3 sphereOrigin, float radius, const Camera camera, SamplerState samplerHiz,
    Texture2D hiz) {

    if (sphereOrigin.z + radius >= -camera.frustumNear)
        return true;

    const float minx = sphereOrigin.x - radius;
    const float maxx = sphereOrigin.x + radius;
    const float miny = sphereOrigin.y - radius;
    const float maxy = sphereOrigin.y + radius;

    float4 aabb = float4(minx, miny, maxx, maxy) *
        float4(camera.projectionWidth, camera.projectionHeight, camera.projectionWidth, camera.projectionHeight) +  
        float4(camera.projectionBiasX, camera.projectionBiasY, camera.projectionBiasX, camera.projectionBiasY);     
    // clip space -> uv space
    aabb = aabb.xyzw * 0.5f + float4(0.5f);

    const float width  = (aabb.z - aabb.x) * camera.hizResolution.x;
    const float height = (aabb.y - aabb.w) * camera.hizResolution.y;

    const float level = ceil(log2(max(width, height)));

    const float depth = hiz.SampleLevel(samplerHiz, (aabb.xy + aabb.zw) * 0.5f, level).r;

    const float coeff = 1.0f / (camera.frustumFar - camera.frustumNear);
    const float projectedDepth = coeff * ((sphereOrigin.z + radius) + camera.frustumFar);

    return projectedDepth >= depth;
}

public bool isBackfaceTriangleVisible(float4 a, float4 b, float4 c) {
    return determinant(float3x3(a.xyw, b.xyw, c.xyw)) <= 0;
}

public bool isScreenSizeVisible(float4 aabb, float width, float height) {
    aabb = aabb * float4(width, height, width, height) + float4(-0.5f, 0.5f, 0.5f, -0.5f);
    return round(aabb.x) != round(aabb.z) && round(aabb.y) != round(aabb.w);
}

public bool isFrustumTriangleVisible(float4 aabb) {
    return aabb.z >= 0 && aabb.x <= 1 && aabb.y >= 0 && aabb.w <= 1;
}

public bool isOcclusionTriangleVisible(float4 aabb, float z, const Camera camera, SamplerState samplerHiz,
    Texture2D hiz) {
    const float width = (aabb.z - aabb.x) * camera.hizResolution.x;
    const float height = (aabb.y - aabb.w) * camera.hizResolution.y;
    const float level = ceil(log2(max(width, height)));

    const float depth =  hiz.SampleLevel(samplerHiz, (aabb.xy + aabb.zw) * 0.5f, level).r;

    return depth <= z;
}

public bool isVisible(const ViewInfo view, float3 origin, float radius) {
    bool visible = true;
    if (view.camera.isOrthographic())
        visible = visible && isFrustumVisibleOrthographic(origin, radius, view.camera);        
    else
        visible = visible && isFrustumVisible(origin, radius, view.camera);

    return visible;
}

public bool isReocclusionVisible(const ViewInfo view, SamplerState sampler, Texture2D hiz, float3 origin, float radius) {
    bool visible = true;
    if (view.camera.isOrthographic())
        visible = visible &&
            isFrustumVisibleOrthographic(origin, radius, view.camera) &&
            isOcclusionVisibleOrthographic(origin, radius, view.camera, sampler, hiz);
    else
        visible = visible &&
            isFrustumVisible(origin, radius, view.camera) &&
            isOcclusionVisible(origin, radius, view.camera, sampler, hiz);        

    return visible;
}