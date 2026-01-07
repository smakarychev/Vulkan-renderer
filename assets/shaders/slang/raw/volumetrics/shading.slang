implementing volumetrics;

import "light/light";

#if CLOUDS_PRIMARY_VIEW
#define CLOUDS_LIGHT_MARCH_SAMPLE_COUNT 8
#else // CLOUDS_LIGHT_MARCH_SAMPLE_COUNT
#define CLOUDS_LIGHT_MARCH_SAMPLE_COUNT 6
#endif // CLOUDS_LIGHT_MARCH_SAMPLE_COUNT

/// Returns atmosphere transmittance value at point
/// @param viewPosition position of a shaded point on the surface of Earth which is result of `getViewPosition`
/// @param atmosphere atmosphere settings for current view
/// @param lightDirection normalized direction towards light
/// @param transmittanceLut transmittance lut texture
/// @param sampler edge-clamped linear lut sampler
public float3 getAtmosphereTransmittance(float3 viewPosition, const AtmosphereSettings atmosphere,
    const float3 lightDirection, Texture2D transmittanceLut, SamplerState sampler) {

    const float r = length(viewPosition);
    const float3 up = viewPosition / r;

    const float surfaceShadow = 
        getVisibility(atmosphere, viewPosition, lightDirection, float3(0.0f) + PLANET_RADIUS_OFFSET_KM * up);

    const float mu = dot(up, lightDirection);
    const float2 transmittanceUv = transmittanceUvFromRMu(atmosphere, r, dot(up, lightDirection));

    return surfaceShadow * transmittanceLut.SampleLevel(sampler, transmittanceUv, 0.0f).rgb;
}

/// Returns atmosphere aerial perspective value at point
/// @param depth linear depth of shaded point in meters
/// @param screenUv screen uv coordinates of shaded point
/// @param aerialPerspectiveLut aerial perspective lut texture
/// @param sampler edge-clamped linear lut sampler
public float4 getAerialPerspective(float depth, float2 screenUv, Texture3D aerialPerspectiveLut, SamplerState sampler) {
    float slice = aerialPerspectiveMetersToSlice(depth);
    float weight = 1.0f;
    if (slice < 0.5f) {
        // fade to 0 at 0 depth
        weight = saturate(slice * 2.0f);
        slice = 0.5f;
    }
    const float aerialSliceW = sqrt(slice / AERIAL_PERSPECTIVE_LUT_RES);

    return weight * aerialPerspectiveLut.SampleLevel(sampler, float3(screenUv, aerialSliceW), 0.0f);
}

public struct HeightFogShadingParameters {
    public float fogStartView = 100.0f;
    public float fogStart = 0.0f;
    public float fogEnd = 500.0f;
    public float fogDensity = 5e-4f;
    public AtmosphereLutsSamplingInfo atmosphere;
}

public float4 getHeightFog(const HeightFogShadingParameters parameters, float depth, float3 ro, float3 rd,
    float3 lightDirection, float3 l0) {
    const float3 directional = l0 * getAtmosphereTransmittance((ro + rd * depth) * 1e-3f, 
        parameters.atmosphere.atmosphere,
        lightDirection, parameters.atmosphere.transmittanceLut, parameters.atmosphere.samplerEdge);

    const float3 right = normalize(cross(float3(0, 1, 0), rd));
    const float3 forward = normalize(cross(right, float3(0, 1, 0)));
    const float lightViewCos = normalize(float2(dot(lightDirection, forward), dot(lightDirection, right))).x;
    const float2 skyViewUv = skyViewUvFromZenCos(
        parameters.atmosphere.atmosphere, false, 0.0f, lightViewCos, parameters.atmosphere.atmosphere.surface);
    
    float3 fogColor = 
        parameters.atmosphere.skyviewLut.SampleLevel(parameters.atmosphere.samplerEdge, skyViewUv, 0.0f).rgb;
    const float phase = henyeyGreensteinPhase(dot(rd, lightDirection), 0.6f) / (4.0f * PI);
    const float density = 
        heightFogDensity(parameters, depth, getWorldPositionMeters(ro, parameters.atmosphere.atmosphere.surface), rd);

    return float4(fogColor + directional * phase, density);
}

internal float heightFogDensity(const HeightFogShadingParameters parameters, float depth, float3 ro, float3 rd) {
    const float startFalloff = clamp((depth - parameters.fogStartView) / parameters.fogStartView, 0.0f, 1.0f);
    const float fogFalloffScale = 1.0f / (max(0.01f, parameters.fogEnd - parameters.fogStart));
    const float fogFalloff = 6.907755f * fogFalloffScale;

    const float originHeight = ro.y;
    const float effectiveZ = max(abs(rd.y), 1e-3f);

    const float endLineHeight = depth * rd.y + originHeight;
    const float minLineHeight = min(originHeight, endLineHeight);
    const float heightLineFalloff = max(minLineHeight - parameters.fogStart, 0.0f);

    const float baseHeightFogDistance = clamp((parameters.fogStart - minLineHeight) / effectiveZ, 0.0f, depth);
    const float expFogDistance = depth - baseHeightFogDistance;
    const float expHeightLineIntergral =
        exp(-heightLineFalloff * fogFalloff) *
        (1.0f - exp(-expFogDistance * effectiveZ * fogFalloff)) / (effectiveZ * fogFalloff);

    const float opticalDepthHeightFog = parameters.fogDensity * startFalloff *
        (baseHeightFogDistance + expHeightLineIntergral);
    const float transmittance = exp(-opticalDepthHeightFog);

    return 1.0f - transmittance;
}


public struct CloudsShadingInfo {
    static const float3 CLOUDS_SCATTERING = float3(1.0f * 0.07f);
    static const float CLOUDS_EXTINCTION = 0.07f;

    float3 p;
    float3 l0;
    float3 lightDirection;
    float dl;
    float sampleTransmittance;
    float primaryPhase;
    float secondaryPhase;
    Texture2D transmittanceLut;
    SamplerState transmittanceSampler;
    SH2Irradiance irradiance;
};


internal float cloudLightMarch(const CloudsVPSamplingInfo clouds, float3 p, float3 rd) {
    float density = 0.0f;

    const float sampleCount = CLOUDS_LIGHT_MARCH_SAMPLE_COUNT;
    const float sampleT = 0.5f;

    float lodOffset = 0.5f;
    float lastStep = 0;
    [unroll]
    for (float i = 0.0f; i < sampleCount; i += 1.0f) {
        float t0 = i / sampleCount;
        float t1 = (i + 1) / sampleCount;
        t0 = t0 * t0;
        t1 = t1 * t1;

        const float delta = t1 - t0;
        const float t = t0 + delta * sampleT + delta * clouds.lightBlueNoise;
        const float shadowT = 384.0f * t;
        const float stepSize = shadowT - lastStep;
        lastStep = shadowT;
        const float3 samplePoint = p + rd * shadowT;

        const CloudLayerVPInfo layer = CloudLayerVPInfo.sampleAt(samplePoint, clouds);
        density += layer.samplerDensityAt(samplePoint, lodOffset, false, clouds) * stepSize;
        lodOffset += 0.5f;
    }

    return density;
}

internal float3 getCloudDirectScattering(const AtmosphereSettings atmosphere, const CloudLayerVPInfo layer, 
    const CloudsShadingInfo shading) {

    const float attenuation = exp(-CloudsShadingInfo.CLOUDS_EXTINCTION * shading.dl);
    float3 transmittance = getAtmosphereTransmittance(shading.p * 1e-3f, atmosphere, shading.lightDirection, 
        shading.transmittanceLut, shading.transmittanceSampler);
    const float3 primaryScattering = transmittance * attenuation * shading.primaryPhase;

    float msVolume = remap01(layer.dimensionalProfile, 0.1f, 1.0f) * 16;
    msVolume *= pow(layer.coverage * layer.type, 0.25f);
    msVolume *= pow(attenuation, 0.2);
    msVolume *= pow(layer.relativeHeight, 2.0);

    const float3 secondaryScattering = transmittance * float3(msVolume) * shading.secondaryPhase;

    return (secondaryScattering + primaryScattering) * shading.l0;
}

internal float3 getCloudAmbientScattering(const CloudLayerVPInfo layer, const CloudsShadingInfo shading) {
    const float verticalGradient = remap01(layer.relativeHeight, 0.2f, 0.6f);
    const float ambientMultiplier = 0.5f;
    const float3 ambient = shading.irradiance.shadeConstant() * ambientMultiplier * verticalGradient;
    const float scattering = pow(1.0f - layer.dimensionalProfile, 0.5f);

    return ambient * scattering;
}

internal float3 getCloudLightEnergy(const AtmosphereSettings atmosphere, const CloudLayerVPInfo layer, 
    const CloudsShadingInfo shading) {

    float3 lightEnergy = 
        getCloudDirectScattering(atmosphere, layer, shading) + getCloudAmbientScattering(layer, shading);
    lightEnergy = 
        (lightEnergy - lightEnergy * shading.sampleTransmittance) / max(1e-7f, CloudsShadingInfo.CLOUDS_EXTINCTION);
    lightEnergy = lightEnergy * CloudsShadingInfo.CLOUDS_SCATTERING;

    return lightEnergy;
}