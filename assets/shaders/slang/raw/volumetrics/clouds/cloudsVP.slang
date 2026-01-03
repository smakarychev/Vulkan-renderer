implementing volumetrics;

struct CloudsVPShadowSamplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct CloudsVPShadowResources {
    CloudsVPResources clouds;
    ConstantBuffer<ViewInfo> view;
    RWTexture2D<float3> shadow;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void cloudsVPShadow(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<CloudsVPShadowSamplers> samplers,
    uniform ParameterBlock<CloudsVPShadowResources> resources) {

    float2 shadowSize;
    resources.shadow.GetDimensions(shadowSize.x, shadowSize.y);
    const float2 shadowSizeInverse = 1.0f / shadowSize;
    const float2 uv = (float2(dispatchThreadID) + 0.5f) * shadowSizeInverse;

    const float3 clip = float3(uv * 2.0f - 1.0f, -1.0f);
    float4 unprojected = mul(float4(clip, 1.0f), resources.view.camera.inverseProjection);
    const float3 rd = normalize(mul(float4(unprojected.xyz, 0.0f), resources.view.camera.inverseView).xyz);
    const float3 ro = getViewPositionMeters(resources.view.camera.position, resources.view.atmosphere.surface * 1e+3f);

    CloudsVPSamplingInfo clouds;
    clouds.view = resources.view;
    clouds.sampler = samplers.sampler;
    clouds.clouds = resources.clouds;
    clouds.windDirection = float3(
        cos(resources.clouds.parameters.windAngle),
        -resources.clouds.parameters.windUprightAmount,
        sin(resources.clouds.parameters.windAngle)
    );
    resources.clouds.coverage.GetDimensions(clouds.cloudMapSizeInverse.x, clouds.cloudMapSizeInverse.y);
    clouds.cloudMapSizeInverse = 1.0f / clouds.cloudMapSizeInverse;
    clouds.earthCenter = CLOUDS_EARTH_CENTER_DEFAULT;
    clouds.cloudMinMax = float2(CLOUDS_MIN_RADIUS_DEFAULT_M, CLOUDS_MAX_RADIUS_DEFAULT_M);

    resources.shadow[dispatchThreadID] = cloudsShadow(uv, ro, rd, clouds);
}

float3 cloudsShadow(float2 uv, float3 ro, float3 rd, const CloudsVPSamplingInfo clouds) {
    const float3 surfaceNormal = normalize(ro);
    const float angleZenith = clamp(dot(rd, surfaceNormal), 0.0f, 1.0f);
    const float maxSteps = lerp(16.0f, 32.0f, 1.0f - angleZenith);

    static const float MIN_LOD = 1.0f;
    static const bool SAMPLE_DETAIL = true;
    static const float MIN_TRANSMITTANCE = 5e-3f;

    float depthT = CLOUDS_DEPTH_MAX;

    Intersection cloudIntersection = getCloudLayerIntersection(ro, rd, clouds.earthCenter, clouds.cloudMinMax);
    Intersection earthIntersection =
        intersectSphere(ro, rd, CLOUDS_EARTH_CENTER_DEFAULT, CLOUDS_EARTH_RADIUS_DEFAULT_M);
    if (earthIntersection.t != 0 && earthIntersection.t < cloudIntersection.t + cloudIntersection.depth) {
        return float3(0);
    }
    if (cloudIntersection.t > depthT) {
        return float3(0);
    }
    if (cloudIntersection.depth == 0.0f) {
        return float3(0);
    }

    cloudIntersection.depth = min(cloudIntersection.depth, depthT);
    const float maxDistance = cloudIntersection.t > 256 * 64.0f ? 30000.0f : 256 * 64.0f;
    cloudIntersection.depth = min(cloudIntersection.depth, maxDistance);

    const float stepSize = cloudIntersection.depth / maxSteps;
    const float3 step = stepSize * rd;
    float3 t = ro + cloudIntersection.t * rd;
    
    float frontDepth = CLOUDS_DEPTH_MAX;
    float totalDepth = 0;
    float totalExtinction = 0;
    float inCloudSamples = 0;
    
    for (float i = 0; i < maxSteps; i++) {
        const CloudLayerVPInfo layer = CloudLayerVPInfo.sampleAt(t, clouds);
        const float lod = clamp(i * stepSize / maxDistance, MIN_LOD, 6.0f);

        float sampleDensity = layer.samplerDensityAt(t, lod, SAMPLE_DETAIL, clouds);

        if (sampleDensity > 0.0f) {
            frontDepth = min(frontDepth, stepSize * i);
            const float ds = sampleDensity * stepSize;
            totalExtinction += sampleDensity;
            totalDepth += ds;
            inCloudSamples += 1;
        }

        t += step;
    }

    return float3(
        cloudIntersection.t + frontDepth,
        totalExtinction * CLOUDS_EXTINCTION_DEFAULT / max(inCloudSamples, 1), 
        totalDepth);
}