implementing volumetrics;

import "core/lib";

[StandaloneType("cloudsNoiseParameters")]
public struct CloudsNoiseParameters {
    [DefaultValue("0.52f")]
    public float perlinCoverageMin;

    [DefaultValue("1.2f")]
    public float perlinCoverageMax;

    [DefaultValue("0.255f")]
    public float worleyCoverageMin;

    [DefaultValue("1.16f")]
    public float worleyCoverageMax;

    [DefaultValue("0.8f")]
    public float perlinWorleyFraction;

    [DefaultValue("0.61f")]
    public float noiseDensityBias;
}

struct CloudShapeNoiseResources {
    RWTexture3D shape;
    ConstantBuffer<CloudsNoiseParameters> parameters;
}

[SpecializationConstant] const bool IS_HIGH_FREQUENCY = false;

[shader("compute")]
[numthreads(8, 8, 8)]
void cloudShapeNoise(
    uint3 dispatchThreadID : SV_DispatchThreadID,
    uniform ParameterBlock<CloudShapeNoiseResources> resources) {

    float3 textureSize;
    resources.shape.GetDimensions(textureSize.x, textureSize.y, textureSize.z);
    const float3 uv = (float3(dispatchThreadID) + 0.5f) / textureSize;
    if (
        dispatchThreadID.x >= textureSize.x ||
        dispatchThreadID.y >= textureSize.y ||
        dispatchThreadID.z >= textureSize.z)
        return;

    resources.shape[dispatchThreadID] = IS_HIGH_FREQUENCY ? 
        highFrequencyShape(uv) : lowFrequencyShape(resources.parameters, uv);
}

struct CloudCurlNoiseResources {
    RWTexture2D curl;
}

[shader("compute")]
[numthreads(8, 8, 8)]
void cloudCurlNoise(
    uint2 dispatchThreadID : SV_DispatchThreadID,
    uniform ParameterBlock<CloudCurlNoiseResources> resources) {

    float2 textureSize;
    resources.curl.GetDimensions(textureSize.x, textureSize.y);
    const float2 uv = (float2(dispatchThreadID) + 0.5f) / textureSize;
    if (
        dispatchThreadID.x >= textureSize.x ||
        dispatchThreadID.y >= textureSize.y)
        return;

    const float curlNoiseFrequency = 4.0f;
    resources.curl[dispatchThreadID] = float4(encodeCurl(noise.curl(float3(uv, 0.0f), curlNoiseFrequency)), 1.0f);
}

struct CloudCoverageNoiseResources {
    RWTexture2D<float> coverage;
    ConstantBuffer<CloudsNoiseParameters> parameters;
}

[shader("compute")]
[numthreads(8, 8, 8)]
void cloudCoverageNoise(
    uint2 dispatchThreadID : SV_DispatchThreadID,
    uniform ParameterBlock<CloudCoverageNoiseResources> resources) {

    float2 textureSize;
    resources.coverage.GetDimensions(textureSize.x, textureSize.y);
    const float2 uv = (float2(dispatchThreadID) + 0.5f) / textureSize;
    if (
        dispatchThreadID.x >= textureSize.x ||
        dispatchThreadID.y >= textureSize.y)
        return;

    const float perlinFrequency = 8.0f;
    const float worleyFrequency = 4.0f;
    float perlinWorley = perlinWorley(
        float3(uv, perlinFrequency * 512),
        perlinFrequency, worleyFrequency,
        float2(resources.parameters.perlinCoverageMin, resources.parameters.perlinCoverageMax),
        float2(resources.parameters.worleyCoverageMin, resources.parameters.worleyCoverageMax),
        resources.parameters.perlinWorleyFraction);
    perlinWorley = remap01(perlinWorley, resources.parameters.noiseDensityBias, 1.0f);

    resources.coverage[dispatchThreadID] = perlinWorley;
}

struct CloudProfileNoiseResources {
    RWTexture2D profile;
}

[shader("compute")]
[numthreads(8, 8, 8)]
void cloudProfileNoise(
    uint2 dispatchThreadID : SV_DispatchThreadID,
    uniform ParameterBlock<CloudProfileNoiseResources> resources) {

    float2 textureSize;
    resources.profile.GetDimensions(textureSize.x, textureSize.y);
    const float2 uv = (float2(dispatchThreadID) + 0.5f) / textureSize;
    if (
        dispatchThreadID.x >= textureSize.x ||
        dispatchThreadID.y >= textureSize.y)
        return;

    const float perlinFrequency = 4.0f;
    const float typeTop = remap01(noise.perlin3(float3(uv, 0), perlinFrequency, true), 0.2f, 1.4f);
    const float typeBottom = remap01(noise.perlin3(float3(uv, 256), perlinFrequency, true), 0.45f, 1.6f);
    const float topHeight = 1.0f;
    const float bottomHeight = 0.0f;

    resources.profile[dispatchThreadID] = float4(typeTop, typeBottom, topHeight, bottomHeight);
}

float perlinWorley(float3 p, float pFreq, float wFreq, float2 pMinMax, float2 wMinMax, float mixMultiplier) {
    const float perlin = remap01(noise.perlin7(p, pFreq, true), pMinMax.x, pMinMax.y);
    const float worley = remap01(noise.worley3(p, wFreq), wMinMax.x, wMinMax.y);

    return remapClamp(worley, 0.0f, 1.0f, perlin * mixMultiplier, 1.0f);
}

float lowFrequencyShape(const CloudsNoiseParameters parameters, float3 uv) {
    const float perlinFrequency = 8.0f;
    const float worleyFrequency = 4.0f;
    const float perlinWorley = perlinWorley(
        uv,
        perlinFrequency, worleyFrequency,
        float2(parameters.perlinCoverageMin, parameters.perlinCoverageMax),
        float2(parameters.worleyCoverageMin, parameters.worleyCoverageMax),
        parameters.perlinWorleyFraction
    );

    const float worley0 = noise.worley(uv, worleyFrequency * 2.0f);
    const float worley1 = noise.worley(uv, worleyFrequency * 4.0f);
    const float worley2 = noise.worley(uv, worleyFrequency * 8.0f);
    const float worley3 = noise.worley(uv, worleyFrequency * 16.0f);

    const float worleyFbm0 = worley0 * 0.625f + worley1 * 0.25f + worley2 * 0.125f;
    const float worleyFbm1 = worley1 * 0.625f + worley2 * 0.25f + worley3 * 0.125f;
    const float worleyFbm2 = worley2 * 0.75f + worley3 * 0.25f;

    const float lowFreqFbm = worleyFbm0 * 0.625f + worleyFbm1 * 0.25f + worleyFbm2 * 0.125f;
    float baseShape = perlinWorley;

    return remap01(baseShape, lowFreqFbm - 1.0f, 1.0f);
}

float lowFrequencyShapeAlligator(const CloudsNoiseParameters parameters, float3 uv) {
    const float perlinFrequency = 8.0f;
    const float alligatorFrequency = 4.0f;
    const float curlyAlligator = remap01(noise.curlyAlligator(uv, alligatorFrequency),
        parameters.worleyCoverageMin, parameters.worleyCoverageMax);

    const float alligator0 = noise.alligator(uv, alligatorFrequency * 2.0f);
    const float alligator1 = noise.alligator(uv, alligatorFrequency * 4.0f);
    const float alligator2 = noise.alligator(uv, alligatorFrequency * 8.0f);
    const float alligator3 = noise.alligator(uv, alligatorFrequency * 16.0f);

    const float alligatorFbm0 = alligator0 * 0.625f + alligator1 * 0.25f + alligator2 * 0.125f;
    const float alligatorFbm1 = alligator1 * 0.625f + alligator2 * 0.25f + alligator3 * 0.125f;
    const float alligatorFbm2 = alligator2 * 0.75f +  alligator3 * 0.25f;

    const float lowFreqFbm = alligatorFbm0 * 0.625f + alligatorFbm1 * 0.25f + alligatorFbm2 * 0.125f;
    return remap01(curlyAlligator, 0, 1.0f);
}

float highFrequencyShape(float3 uv) {
    const float worleyFrequency = 2.0f;

    const float worley0 = noise.worley(uv, worleyFrequency * 1.0f);
    const float worley1 = noise.worley(uv, worleyFrequency * 2.0f);
    const float worley2 = noise.worley(uv, worleyFrequency * 4.0f);
    const float worley3 = noise.worley(uv, worleyFrequency * 8.0f);

    const float worleyFbm0 = worley0 * 0.625f + worley1 * 0.25f + worley2 * 0.125f;
    const float worleyFbm1 = worley1 * 0.625f + worley2 * 0.25f + worley3 * 0.125f;
    const float worleyFbm2 = worley2 * 0.75f + worley3 * 0.25f;

    return clamp(worleyFbm0 * 0.625f + worleyFbm1 * 0.25f + worleyFbm2 * 0.125f, 0.0f, 1.0f);
}

float highFrequencyShapeAlligator(float3 uv) {
    const float alligatorFrequency = 2.0f;

    const float alligator0 = noise.alligator(uv, alligatorFrequency * 1.0f);
    const float alligator1 = noise.alligator(uv, alligatorFrequency * 2.0f);
    const float alligator2 = noise.alligator(uv, alligatorFrequency * 4.0f);
    const float alligator3 = noise.alligator(uv, alligatorFrequency * 8.0f);

    const float alligatorFbm0 = alligator0 * 0.625f + alligator1 * 0.25f + alligator2 * 0.125f;
    const float alligatorFbm1 = alligator1 * 0.625f + alligator2 * 0.25f + alligator3 * 0.125f;
    const float alligatorFbm2 = alligator2 * 0.75f  + alligator3 * 0.25f;

    return clamp(alligatorFbm0 * 0.625f + alligatorFbm1 * 0.25f + alligatorFbm2 * 0.125f, 0.0f, 1.0f);
}
