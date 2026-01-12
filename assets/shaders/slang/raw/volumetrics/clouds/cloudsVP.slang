implementing volumetrics;

import "utility/gaussianBlurSep";

struct CloudsVPRenderSamplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;

    [ImmutableSampler(SamplerFlags.ClampEdge | SamplerFlags.Linear)]
    SamplerState samplerEdge;

    [ImmutableSampler(SamplerFlags.ReductionMin)]
    SamplerState samplerMin;
}

struct CloudsVPRenderResources {
    CloudsVPResources clouds;
    RWTexture2D<float4> color;
    ConstantBuffer<ViewInfo> view;
    ConstantBuffer<SH2Irradiance> irradiance;

#if CLOUDS_PRIMARY_VIEW
    Texture2D<float> depth;
    Texture2D<float2> depthMinMax;
    Texture3D aerialPerspectiveLut;
    RWTexture2D<float2> outDepth;
#endif // CLOUDS_PRIMARY_VIEW
}


[shader("compute")]
[numthreads(8, 8, 1)]
void cloudsVPRender(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<CloudsVPRenderSamplers> samplers,
    uniform ParameterBlock<CloudsVPRenderResources> resources,
    uniform ParameterBlock<TextureHeap> textures) {

    uint2 coord = dispatchThreadID;
    uint2 outCoord = coord;
    float2 imageSize;
    resources.color.GetDimensions(imageSize.x, imageSize.y);
#if REPROJECTION
    coord *= CLOUDS_REPROJECTION_RES;
    coord += CLOUDS_REPROJECTION_OFFSETS[resources.view.frameNumberU32 % CLOUDS_REPROJECTION_RES_2D];
    imageSize *= CLOUDS_REPROJECTION_RES;
#endif // REPROJECTION

    const float2 uv = (float2(coord) + 0.5f) / imageSize;
    if (coord.x >= imageSize.x || coord.y >= imageSize.y) {
        return;
    }

    const float3 clip = float3(uv * 2.0f - 1.0f, -1.0f);
    float4 unprojected = mul(float4(clip, 1.0f), resources.view.camera.inverseProjection);
    const float3 rd = normalize(mul(float4(unprojected.xyz, 0.0f), resources.view.camera.inverseView).xyz);
    const float3 ro = getViewPositionMeters(resources.view.camera.position, resources.view.atmosphere.surface * 1e+3f);

#if CLOUDS_PRIMARY_VIEW
    static const float MIN_MAX_DEPTH_MIPMAP = 5.0f;
    const float depth = resources.depthMinMax.SampleLevel(samplers.samplerMin, uv, MIN_MAX_DEPTH_MIPMAP).r;
    const float linearDepth = -Camera.linearizeReverseZInf(max(depth, 1e-12f), resources.view.camera.near);
    const float shapeBlueNoise = noise.blueSample128(
        textures.textures[resources.view.shading.blueNoise128], coord, resources.view.frameNumber);
    const float lightBlueNoise = noise.blueSample128(
        textures.textures[resources.view.shading.blueNoise128], coord + 1, resources.view.frameNumber);
#else // CLOUDS_PRIMARY_VIEW
    const float linearDepth = CLOUDS_MAX_RADIUS_DEFAULT_M;
    const float shapeBlueNoise = 0.0f;
    const float lightBlueNoise = 0.0f;
#endif // CLOUDS_PRIMARY_VIEW


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
    clouds.shapeBlueNoise = shapeBlueNoise;
    clouds.lightBlueNoise = lightBlueNoise;

    AtmosphereLutsSamplingInfo atmosphere;
    atmosphere.atmosphere = resources.view.atmosphere;
    atmosphere.transmittanceLut = textures.textures[resources.view.shading.transmittanceLut];
    atmosphere.skyviewLut = textures.textures[resources.view.shading.skyViewLut];
#if CLOUDS_PRIMARY_VIEW
    atmosphere.aerialPerspectiveLut = resources.aerialPerspectiveLut;
#endif // CLOUDS_PRIMARY_VIEW
    atmosphere.samplerEdge = samplers.samplerEdge;

    const CloudsRenderInfo cloudsRender = 
        renderClouds(uv, ro, rd, linearDepth, clouds, atmosphere, resources.irradiance);

#if CLOUDS_PRIMARY_VIEW
    resources.outDepth[outCoord] = float2(cloudsRender.depth, resources.depth[coord]);
#endif // CLOUDS_PRIMARY_VIEW
    resources.color[outCoord] = cloudsRender.color;
}

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
    const float2 uv = (float2(dispatchThreadID) + 0.5f) / shadowSize;

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

#ifndef VERTICAL
#define VERTICAL 1
#endif // VERTICAL

#if VERTICAL
#define BLUR_DIRECTION GaussianBlurDirection.Vertical
#else  // VERTICAL
#define BLUR_DIRECTION GaussianBlurDirection.Horizontal
#endif // VERTICAL

struct CloudsVPEnvironmentBlurSamplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct CloudsVPEnvironmentBlurResources {
    Texture2D<float4> clouds;
    RWTexture2D<float4> cloudsAtmosphereBlurred;
}

struct CloudsVPEnvironmentBlur<let BlurDirection : GaussianBlurDirection> : IGaussianBlur<float4> {
    CloudsVPEnvironmentBlurSamplers samplers;
    CloudsVPEnvironmentBlurResources resources;

    float4 readImage(float2 uv, float scale) {
        return resources.clouds.SampleLevel(samplers.sampler, uv, 0) * scale;
    }
    void storeImage(uint2 coord, float4 val) {
        if (BlurDirection == GaussianBlurDirection.Vertical) {
            resources.cloudsAtmosphereBlurred[coord] = val;
        }
        else {
            const float4 atmosphere = resources.cloudsAtmosphereBlurred[coord];
            resources.cloudsAtmosphereBlurred[coord] = 
                float4(atmosphere.rgb * val.a + val.rgb, 1.0f - val.a + atmosphere.a * val.a);
        }
    }
    float2 imageSize() {
        float2 size;
        resources.clouds.GetDimensions(size.x, size.y);

        return size;
    }
}

[shader("compute")]
[numthreads(8, 8, 1)]
void cloudsVPEnvironmentBlur(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<CloudsVPEnvironmentBlurSamplers> samplers,
    uniform ParameterBlock<CloudsVPEnvironmentBlurResources> resources) {
    CloudsVPEnvironmentBlur<BLUR_DIRECTION> environmentBlur =
        CloudsVPEnvironmentBlur<BLUR_DIRECTION>(samplers, resources);
    gaussianBlur.blur5<BLUR_DIRECTION>(environmentBlur, dispatchThreadID);
}

struct CloudsVPShadowBlurSamplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct CloudsVPShadowBlurResources {
    Texture2D<float3> shadow;
    RWTexture2D<float3> shadowBlurred;
}

struct CloudsVPShadowBlur : IGaussianBlur<float3> {
    CloudsVPShadowBlurSamplers samplers;
    CloudsVPShadowBlurResources resources;

    float3 readImage(float2 uv, float scale) {
        return resources.shadow.SampleLevel(samplers.sampler, uv, 0) * scale;
    }
    void storeImage(uint2 coord, float3 val) {
        resources.shadowBlurred[coord] = val;
    }
    float2 imageSize() {
        float2 size;
        resources.shadow.GetDimensions(size.x, size.y);

        return size;
    }
}

[shader("compute")]
[numthreads(8, 8, 1)]
void cloudsVPShadowBlur(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<CloudsVPShadowBlurSamplers> samplers,
    uniform ParameterBlock<CloudsVPShadowBlurResources> resources) {
    CloudsVPShadowBlur shadowBlur = CloudsVPShadowBlur(samplers, resources);
    gaussianBlur.blur5<BLUR_DIRECTION>(shadowBlur, dispatchThreadID);
}

struct CloudsRenderInfo {
    float4 color;
    float depth;
}

CloudsRenderInfo renderClouds(float2 screenUv, float3 ro, float3 rd, float maxDepth, 
    const CloudsVPSamplingInfo clouds, const AtmosphereLutsSamplingInfo atmosphere, const SH2Irradiance irradiance) {
    
    const float angleZenith = saturate(dot(rd, float3(0.0f, 1.0f, 0.0f)));
#if CLOUDS_PRIMARY_VIEW
    const float maxSteps = lerp(64.0f, 158.0f, 1.0f - angleZenith);
    const float MIN_LOD = 0.0f;
    const bool SAMPLE_DETAIL = true;
#else // CLOUDS_PRIMARY_VIEW
    const float maxSteps = lerp(32.0f, 48.0f, 1.0f - angleZenith);
    const float MIN_LOD = 3.0f;
    const bool SAMPLE_DETAIL = false;
#endif // CLOUDS_PRIMARY_VIEW
    const float MIN_TRANSMITTANCE = 5e-3f;

    float depthT = maxDepth;

    Intersection cloudIntersection = getCloudLayerIntersection(ro, rd, clouds.earthCenter, clouds.cloudMinMax);
    Intersection earthIntersection =
        intersectSphere(ro, rd, CLOUDS_EARTH_CENTER_DEFAULT, CLOUDS_EARTH_RADIUS_DEFAULT_M);
    if (earthIntersection.t != 0 && earthIntersection.t < cloudIntersection.t + cloudIntersection.depth) {
        return CloudsRenderInfo(float4(0.0f, 0.0f, 0.0f, 1.0f), CLOUDS_DEPTH_MAX);
    }
    if (cloudIntersection.t > depthT) {
        return CloudsRenderInfo(float4(0.0f, 0.0f, 0.0f, 1.0f), CLOUDS_DEPTH_MAX);
    }
    if (cloudIntersection.depth == 0.0f) {
        return CloudsRenderInfo(float4(0.0f, 0.0f, 0.0f, 1.0f), CLOUDS_DEPTH_MAX);
    }

    cloudIntersection.depth = min(cloudIntersection.depth, depthT);
    const float maxDistance = cloudIntersection.t > 256 * 64.0f ? 30000.0f : 256 * 64.0f;
    cloudIntersection.depth = min(cloudIntersection.depth, maxDistance);

    float frontDepth = CLOUDS_DEPTH_MAX;
    float totalDepth = 0;
    float totalExtinction = 0;
    float inCloudSamples = 0;

    const float stepSize = cloudIntersection.depth / maxSteps;
    float t = cloudIntersection.t + stepSize * clouds.shapeBlueNoise;
    float depthWeightedTransmittance = 0.0f;
    float depthWeightedTransmittanceTotal = 0.0f;
    float transmittance = 1.0f;
    float3 inScattering = float3(0.0f);

    const float3 l0 = clouds.view.shading.primaryDirectionalLightColor.rgb *
        clouds.view.shading.primaryDirectionalLightIntensity;
    const float3 lightDirection = -clouds.view.shading.primaryDirectionalLightDirection;
    const float mu = dot(rd, lightDirection);

    const float phase = max(
        henyeyGreensteinPhase(mu, clouds.clouds.parameters.hGEccentricity), 
        0.4f * henyeyGreensteinPhase(mu, 0.88f));

    float inCloud = 0.0;
    float previousDensity = 0.0f;
    uint cloudMisses = 0;

    for (float i = 0; i < maxSteps && transmittance > MIN_TRANSMITTANCE; i++) {
        const float3 p = ro + t * rd;
        const CloudLayerVPInfo layer = CloudLayerVPInfo.sampleAt(p, clouds);
        const float lod = clamp(i * stepSize / maxDistance, MIN_LOD, 6.0f);

        if (inCloud > 0.0) {
            float sampleDensity = layer.samplerDensityAt(p, lod, SAMPLE_DETAIL, clouds);

            if (sampleDensity == 0.0f && previousDensity == 0.0f) {
                cloudMisses += 1;
            }
            if (cloudMisses > 10) {
                inCloud = 0.0f;
                cloudMisses = 0;
            }
            else if (sampleDensity > 0.0f) {
                const float ds = sampleDensity * stepSize;
                const float sampleTransmittance = exp(-ds * CloudsShadingInfo.CLOUDS_EXTINCTION);

                CloudsShadingInfo shading;
                shading.p = p;
                shading.lightDirection = lightDirection;
                shading.dl = cloudLightMarch(clouds, p, lightDirection);
                shading.l0 = l0;
                shading.sampleTransmittance = sampleTransmittance;
                shading.primaryPhase = phase;
                shading.secondaryPhase = henyeyGreensteinPhase(mu, clouds.clouds.parameters.hGBackwardEccentricity);
                shading.irradiance = irradiance;
                shading.transmittanceLut = atmosphere.transmittanceLut;
                shading.transmittanceSampler = atmosphere.samplerEdge;
                const float3 lightEnergy = getCloudLightEnergy(atmosphere.atmosphere, layer, shading);

                inScattering += transmittance * lightEnergy;
                depthWeightedTransmittance += transmittance * length(p - ro);
                depthWeightedTransmittanceTotal += transmittance;

                transmittance *= sampleTransmittance;
            }

            t += stepSize;

            previousDensity = sampleDensity;
        } else {
            inCloud = layer.samplerDensityAt(p, lod + 1.0f, false, clouds);
            if (inCloud == 0) {
                t += stepSize * 2.0f;
                continue;
            } else {
                t -= stepSize;
            }
        }
    }

    const float avgDepth = depthWeightedTransmittance / max(depthWeightedTransmittanceTotal, 1e-7f);

    inScattering = expose(inScattering);

#if CLOUDS_PRIMARY_VIEW
    const float4 aerialPerspective = getAerialPerspective(avgDepth, screenUv,
        atmosphere.aerialPerspectiveLut, atmosphere.samplerEdge);
    inScattering = (1.0f - aerialPerspective.a) * inScattering + aerialPerspective.rgb * (1.0f - transmittance);
#endif // CLOUDS_PRIMARY_VIEW

    HeightFogShadingParameters fogParameters = HeightFogShadingParameters();
    fogParameters.atmosphere = atmosphere;
    const float4 fog = getHeightFog(fogParameters, avgDepth, ro, rd, lightDirection, l0);
    inScattering = lerp(inScattering, fog.rgb * (1.0f - transmittance), fog.a);

    return CloudsRenderInfo(
        float4(inScattering, transmittance),
        transmittance > 0.99f ? CLOUDS_DEPTH_MAX : avgDepth
    );

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
        totalExtinction * CloudsShadingInfo.CLOUDS_EXTINCTION / max(inCloudSamples, 1), 
        totalDepth
    );
}