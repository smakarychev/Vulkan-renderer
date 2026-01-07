implementing volumetrics;

import "core/viewInfo";

[StandaloneType("cloudsVPParameters")]
public struct CloudsVPParameters {
    [DefaultValue("64.0f")]
    public float cloudMapMetersPerTexel;

    [DefaultValue("1.0f / 4000.0f")]
    public float shapeNoiseScale;

    [DefaultValue("8.0f")]
    public float detailNoiseScaleMultiplier;

    [DefaultValue("0.1f")]
    public float detailNoiseContribution;

    [DefaultValue("3.0f")]
    public float detailNoiseHeightModifier;

    [DefaultValue("0.3f")]
    public float windAngle;

    [DefaultValue("0.15f")]
    public float windSpeed;

    [DefaultValue("0.1f")]
    public float windUprightAmount;

    [DefaultValue("500.0f")]
    public float windHorizontalSkew;

    [DefaultValue("0.3f")]
    public float curlNoiseScaleMultiplier;

    [DefaultValue("0.6f")]
    public float curlNoiseHeight;

    [DefaultValue("0.8f")]
    public float curlNoiseContribution;

    [DefaultValue("0.2f")]
    public float hGEccentricity;

    [DefaultValue("-0.15f")]
    public float hGBackwardEccentricity;

    [DefaultValue("~0u")]
    public uint blueNoiseBindlessIndex;
}

public struct CloudsVPResources {
    public Texture2D<float> coverage;
    public Texture2D<float4> profile;
    public Texture3D<float> lowFrequency;
    public Texture3D<float> highFrequency;
    public Texture2D<float3> curlNoise;
    public ConstantBuffer<CloudsVPParameters> parameters;
}

internal struct CloudsVPSamplingInfo {
    ConstantBuffer<ViewInfo> view;
    SamplerState sampler;
    CloudsVPResources clouds;
    float3 windDirection;
    float2 cloudMapSizeInverse;
    float3 earthCenter;
    float2 cloudMinMax;
    float shapeBlueNoise;
    float lightBlueNoise;
}

/// Vertical profile cloud layer info
internal struct CloudLayerVPInfo {
    float relativeHeight;
    float type;
    float coverage;
    float dimensionalProfile;

    static CloudLayerVPInfo sampleAt(float3 p, const CloudsVPSamplingInfo info) {

        CloudLayerVPInfo layer;
        layer.relativeHeight = ::relativeHeight(p, info.earthCenter, info.cloudMinMax);

        const CloudsVPResources clouds = info.clouds;

        float3 windDirection = float3(
            cos(clouds.parameters.windAngle), -clouds.parameters.windUprightAmount, sin(clouds.parameters.windAngle));

        p.xz += layer.relativeHeight * windDirection.xy * clouds.parameters.windHorizontalSkew;
        p.xz += windDirection.xy * info.view.frameNumber * clouds.parameters.windSpeed;
        const float2 uvPlanar = 
            (p.xz * info.cloudMapSizeInverse / clouds.parameters.cloudMapMetersPerTexel + 1.0f) * 0.5f;
        const float coverage = clouds.coverage.SampleLevel(info.sampler, uvPlanar, 0.0f);
        const float4 profile = clouds.profile.SampleLevel(info.sampler, uvPlanar, 0.0f);
        const float2 gradientBottom = clamp(heightGradientBottom(profile.y) + profile.z, profile.z, profile.w);
        const float2 gradientTop = clamp(heightGradientTop(profile.x) + profile.z, profile.z, profile.w);

        const float verticalProfile = layer.getVerticalProfile(gradientBottom, gradientTop);
        const float dimensionalProfile = verticalProfile * coverage;
        layer.type = profile.x;
        layer.coverage = coverage;
        layer.dimensionalProfile = dimensionalProfile;

        return layer;
    }

    float samplerDensityAt(float3 p, float mipLevel, bool sampleDetail, const CloudsVPSamplingInfo info) {
        const CloudsVPResources clouds = info.clouds;

        p += relativeHeight * info.windDirection * clouds.parameters.windHorizontalSkew;
        p += info.windDirection * info.view.frameNumber * clouds.parameters.windSpeed;

        const float3 lowFrequencySampleP = p * clouds.parameters.shapeNoiseScale;
        float cloudSample = clouds.lowFrequency.SampleLevel(info.sampler, lowFrequencySampleP, mipLevel);
        cloudSample = saturate(cloudSample - (1.0f - dimensionalProfile));

        if (cloudSample > 0.0f && sampleDetail) {
            float3 highFrequencySampleP = lowFrequencySampleP * clouds.parameters.detailNoiseScaleMultiplier;

            const float3 curlSample = decodeCurl(
                clouds.curlNoise.SampleLevel(
                    info.sampler,
                    p.xz * clouds.parameters.shapeNoiseScale * clouds.parameters.curlNoiseScaleMultiplier,
                    0.0f)
            );

            highFrequencySampleP += curlSample *
                saturate(pow(1.0f - relativeHeight, clouds.parameters.curlNoiseHeight)) *
                clouds.parameters.curlNoiseContribution;

            const float highFrequencySample = 
                clouds.highFrequency.SampleLevel(info.sampler, highFrequencySampleP, mipLevel);

            const float highFrequencyModifier = lerp(1.0f - highFrequencySample, highFrequencySample,
                saturate(relativeHeight * clouds.parameters.detailNoiseHeightModifier));

            cloudSample = 
                remap(cloudSample, highFrequencyModifier * clouds.parameters.detailNoiseContribution, 1.0f, 0.0f, 1.0f);
        }

        return max(cloudSample, 0.0f);
    }

    static float3 cloudTypeVec(float type) {
        return float3(1.0f - saturate(type * 2.0f), 1.0f - abs(type - 0.5f) * 2.0f, saturate(2.0f * (type - 0.5f)));
    }

    static float2 heightGradientTop(float type) {
        const float3 typeVec = cloudTypeVec(type);

        const float2 stratus = float2(0.15f, 0.2f);
        const float2 stratocumulus = float2(0.3f, 0.4f);
        const float2 cumulus = float2(0.7f, 0.99f);

        return stratus * typeVec.x + stratocumulus * typeVec.y + cumulus * typeVec.z;
    }

    static float2 heightGradientBottom(float type) {
        const float3 typeVec = cloudTypeVec(type);

        const float2 stratus = float2(0.01f, 0.1f);
        const float2 stratocumulus = float2(0.01f, 0.18f);
        const float2 cumulus = float2(0.01f, 0.06f);

        return stratus * typeVec.x + stratocumulus * typeVec.y + cumulus * typeVec.z;
    }

    float getVerticalProfile(float2 gradientBottom, float2 gradientTop) {
        return
            remapClamp(relativeHeight, gradientBottom.x, gradientBottom.y, 0.0f, 1.0f) *
            saturate(remap(relativeHeight, gradientTop.x, gradientTop.y, 1.0f, 0.0f));
    }
};