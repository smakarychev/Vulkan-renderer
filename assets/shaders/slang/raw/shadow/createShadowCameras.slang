module createShadowCameras;

import "core/viewInfo";
import "shadow";

struct MinMaxBuffer {
    uint min;
    uint max;
}

struct Resources {
    ConstantBuffer<MinMaxBuffer> minMax;
    RWStructuredBuffer<CsmData> csm;
    ConstantBuffer<ViewInfo> view;
    RWStructuredBuffer<ViewInfo> shadowViews[CsmData.MAX_CASCADES];
}

ParameterBlock<Resources> resources;

float4x4 inverseRotationTranslation(float3x3 r, float3 t) {
    float4x4 inv = float4x4(float4(r._11_21_31, 0.0f),
                            float4(r._12_22_32, 0.0f),
                            float4(r._13_23_33, 0.0f),
                            float4(0.0f, 0.0f, 0.0f, 1.0f));
    inv[3][0] = -dot(t, r[0]);
    inv[3][1] = -dot(t, r[1]);
    inv[3][2] = -dot(t, r[2]);
    
    return inv;
}

float4x4 ortho(float l, float b, float r, float t, float n, float f) {
    return float4x4(float4(2.0f / (r - l), 0, 0, 0),
                    float4(0, -2.0f / (t - b), 0, 0),
                    float4(0, 0, -1 / (f - n), 0),
                    float4((l + r) / (l - r), -(t + b)/(b - t), n / (n - f),  1));
}

float4x4 inverse(float4x4 m) {
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0f / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

    return ret;
}

[shader("compute")]
[numthreads(CsmData.MAX_CASCADES, 1, 1)]
void main(uint dispatchThreadID: SV_DispatchThreadID, 
    uniform uint shadowSize,
    uniform uint cascadeCount, 
    uniform float maxShadowDistance,
    uniform float3 lightDirection) {
    
    if (dispatchThreadID >= cascadeCount)
        return;

    Camera camera = resources.view.camera;

    float cascades[CsmData.MAX_CASCADES];
    float f = min(camera.far, maxShadowDistance);
    float n = camera.near;
    const float depthMin = -camera.linearizeReverseZ(asfloat(resources.minMax.max));
    const float depthMax = -camera.linearizeReverseZ(asfloat(resources.minMax.min));
    f = min(f, depthMax);
    n = max(n, depthMin);
    const float ratio = f / n;
    for (uint i = 0; i < cascadeCount; i++) {
        const float p = float(i + 1) / float(cascadeCount);
        const float logScale = n * pow(abs(ratio), p);
        cascades[i] = logScale;
    }

    const uint cascadeIndex = dispatchThreadID;
    const float prevCascade = cascadeIndex == 0 ? depthMin : cascades[cascadeIndex - 1];
    const float cascade = cascades[cascadeIndex];

    float3 frustumWs[8] = {
        float3(0.0f, 0.0f, 0.0f),
        float3(0.0f, 0.0f, 0.0f),
        float3(0.0f, 0.0f, 0.0f),
        float3(0.0f, 0.0f, 0.0f),
        float3(0.0f, 0.0f, 0.0f),
        float3(0.0f, 0.0f, 0.0f),
        float3(0.0f, 0.0f, 0.0f),
        float3(0.0f, 0.0f, 0.0f),
    };

    const float3 nCenter = camera.forward * prevCascade;
    const float3 fCenter = camera.forward * cascade;
    const float tanFov = tan(camera.fov * 0.5f);
    const float nHeight = tanFov * prevCascade;
    const float nWidth = nHeight * camera.aspectRatio;
    const float fHeight = tanFov * cascade;
    const float fWidth = fHeight * camera.aspectRatio;

    frustumWs[0] = camera.position + nCenter - camera.right * nWidth - camera.up * nHeight;
    frustumWs[1] = camera.position + nCenter - camera.right * nWidth + camera.up * nHeight;
    frustumWs[2] = camera.position + nCenter + camera.right * nWidth + camera.up * nHeight;
    frustumWs[3] = camera.position + nCenter + camera.right * nWidth - camera.up * nHeight;
    frustumWs[4] = camera.position + fCenter - camera.right * fWidth - camera.up * fHeight;
    frustumWs[5] = camera.position + fCenter - camera.right * fWidth + camera.up * fHeight;
    frustumWs[6] = camera.position + fCenter + camera.right * fWidth + camera.up * fHeight;
    frustumWs[7] = camera.position + fCenter + camera.right * fWidth - camera.up * fHeight;

    float3 center = float3(0.0f);
    [unroll]
    for(uint i = 0; i < 8; ++i)
        center += frustumWs[i];
    center /= 8.0f;

    const float3 up = abs(lightDirection.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(0.0f, 0.0f, 1.0f);

    // create a temporary view matrix for the light
    float3 cameraPos = center;
    float3x3 cameraRotation;
    cameraRotation[2] = -lightDirection;
    cameraRotation[0] = normalize(cross(up, cameraRotation[2]));
    cameraRotation[1] = cross(cameraRotation[2], cameraRotation[0]);
    float4x4 cameraView = inverseRotationTranslation(cameraRotation, cameraPos);

    float3 minExtents;
    float3 maxExtents;
    // calculate an AABB around the frustum corners
    const float floatMax = 3.402823466e+38F;
    float3 mins = float3(floatMax, floatMax, floatMax);
    float3 maxes = float3(-floatMax, -floatMax, -floatMax);
    [unroll]
    for(uint i = 0; i < 8; i++) {
        const float3 corner = mul(float4(frustumWs[i], 1.0f), cameraView).xyz;
        mins = min(mins, corner);
        maxes = max(maxes, corner);
    }

    minExtents = mins;
    maxExtents = maxes;

     // adjust the min/max to accommodate the filtering size
    const float scale = (shadowSize + 7.0f) / float(shadowSize);
    minExtents.x *= scale;
    minExtents.y *= scale;
    maxExtents.x *= scale;
    maxExtents.y *= scale;
    
    const float3 cascadeExtents = maxExtents - minExtents;
    
    const float3 shadowPos = center - lightDirection * maxExtents.z;
    
    // сome up with a new orthographic camera for the shadow caster
    const float4x4 shadowView = inverseRotationTranslation(cameraRotation, shadowPos);
    const float4x4 shadowProjection = ortho(minExtents.x, minExtents.y, maxExtents.x, maxExtents.y,
        cascadeExtents.z, 0.0f);
    
    const float4x4 shadowViewProjection = mul(shadowView, shadowProjection);

    resources.csm[0].cascades[cascadeIndex] = cascade;
    resources.csm[0].viewProjections[cascadeIndex] = shadowViewProjection;
    resources.csm[0].views[cascadeIndex] = shadowView;
    resources.csm[0].near[cascadeIndex] = 0.0f;
    resources.csm[0].far[cascadeIndex] = cascadeExtents.z;

    resources.shadowViews[cascadeIndex][0].camera.viewProjection = shadowViewProjection;
    resources.shadowViews[cascadeIndex][0].camera.view = shadowView;
    resources.shadowViews[cascadeIndex][0].camera.projection = shadowProjection;
    resources.shadowViews[cascadeIndex][0].camera.inverseViewProjection = inverse(shadowViewProjection);
    resources.shadowViews[cascadeIndex][0].camera.inverseView = inverse(shadowView);
    resources.shadowViews[cascadeIndex][0].camera.inverseProjection = inverse(shadowProjection);
    resources.shadowViews[cascadeIndex][0].camera.near = 0.0f;
    resources.shadowViews[cascadeIndex][0].camera.far = cascadeExtents.z;
    resources.shadowViews[cascadeIndex][0].camera.position = shadowPos;
    resources.shadowViews[cascadeIndex][0].camera.forward = lightDirection;
    resources.shadowViews[cascadeIndex][0].camera.frustumFar = cascadeExtents.z;
    resources.shadowViews[cascadeIndex][0].camera.frustumNear = 0.0f;
    resources.shadowViews[cascadeIndex][0].camera.frustumRightX = shadowProjection[0][0];
    resources.shadowViews[cascadeIndex][0].camera.frustumRightZ = 0.0f;
    resources.shadowViews[cascadeIndex][0].camera.frustumTopY = shadowProjection[1][1];
    resources.shadowViews[cascadeIndex][0].camera.frustumTopZ = 0.0f;
    resources.shadowViews[cascadeIndex][0].camera.projectionWidth = shadowProjection[0][0];
    resources.shadowViews[cascadeIndex][0].camera.projectionHeight = shadowProjection[1][1];
    resources.shadowViews[cascadeIndex][0].camera.projectionBiasX = shadowProjection[3][0];
    resources.shadowViews[cascadeIndex][0].camera.projectionBiasY = shadowProjection[3][1];
    resources.shadowViews[cascadeIndex][0].camera.resolution = float2(shadowSize);
    resources.shadowViews[cascadeIndex][0].camera.hizResolution = float2(shadowSize);
    resources.shadowViews[cascadeIndex][0].camera.viewFlags = 
        (1 << VIEW_CLAMP_DEPTH_BIT) | (1 << VIEW_IS_ORTHOGRAPHIC_BIT);
    
    if (cascadeIndex == 0) {
        resources.csm[0].cascadeCount = cascadeCount;
    }
}
