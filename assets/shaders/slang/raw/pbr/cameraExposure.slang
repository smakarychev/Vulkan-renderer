module cameraExposure;

import "core/attributes";
import "core/viewInfo";
import "core/lib";
import "pbr/pbr";

struct ExposureFromParametersResources {
    RWStructuredBuffer<ViewInfo> view;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void cameraExposureFromParameters(
    uniform float aperture,
    uniform float shutterTime,
    uniform float ISO,
    uniform float evCompensation,
    uniform ParameterBlock<ExposureFromParametersResources> resources) {

    resources.view[0].shading.exposure = convertEV100ToExposure(
        computeEV100(aperture, shutterTime, ISO) - evCompensation);
    resources.view[0].shading.exposureInverse = 1.0f / resources.view[0].shading.exposure;
    resources.view[0].shading.fixedExposureToExposureMultiplier =
        resources.view[0].shading.exposure * resources.view[0].shading.fixedExposureInverse;
    resources.view[0].shading.exposureToFixedExposureMultiplier =
        1.0f / resources.view[0].shading.fixedExposureToExposureMultiplier;
}

static const uint LUMINANCE_HISTOGRAM_BINS_SQRT = 16;
static const uint LUMINANCE_HISTOGRAM_BINS = 128;

struct LiminanceHistogramBinsSamplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct LiminanceHistogramBinsResources {
    ConstantBuffer<ViewInfo> view;
    Texture2D color;

    RWStructuredBuffer<uint> bins;
}

groupshared uint[LUMINANCE_HISTOGRAM_BINS] sBins;

[shader("compute")]
[numthreads(LUMINANCE_HISTOGRAM_BINS_SQRT, LUMINANCE_HISTOGRAM_BINS_SQRT, 1)]
void luminanceHistogram(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uint2 workGroupID: SV_GroupID,
    uint localInvocationIndex: SV_GroupIndex,
    uniform float minLogLuminance,
    uniform float logLuminanceRangeInverse,
    uniform ParameterBlock<LiminanceHistogramBinsResources> resources,
    uniform ParameterBlock<LiminanceHistogramBinsSamplers> samplers) {
        
    if (localInvocationIndex < LUMINANCE_HISTOGRAM_BINS)
        sBins[localInvocationIndex] = 0;
    GroupMemoryBarrierWithGroupSync();
    
    float2 imageSize;
    resources.color.GetDimensions(imageSize.x, imageSize.y);
    float2 imageSizeInverse = 1.0f / imageSize;

    const float3 sample = 
        resources.color.SampleLevel(samplers.sampler, float2(dispatchThreadID + 0.5f) * imageSizeInverse, 0).rgb * 
        resources.view.shading.exposureInverse;
    const float luminance = color.luminance(sample);
    if (luminance < 1e-4f)
        return;
    
    const float luminanceLogScaled = saturate((log2(luminance) - minLogLuminance) * logLuminanceRangeInverse);
    const uint bin = uint((LUMINANCE_HISTOGRAM_BINS - 1) * luminanceLogScaled);

    InterlockedAdd(sBins[bin], 1);
    GroupMemoryBarrierWithGroupSync();

    if (localInvocationIndex < LUMINANCE_HISTOGRAM_BINS)
        InterlockedAdd(resources.bins[localInvocationIndex], sBins[localInvocationIndex]);
}

[StandaloneType("exposureFromHistogramOutput")]
public struct ExposureFromHistogramOutput {
    public float targetEV;
    public float targetCompensatedEV;
    public float currentEV;
    public float firstAccountedEV;
    public float firstNotAccountedEV;
}

struct ExposureFromHistogramResources {
    RWStructuredBuffer<ViewInfo> view;
    RWStructuredBuffer<uint> bins;
    RWStructuredBuffer<ExposureFromHistogramOutput> output;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void cameraExposureFromHistogram(
    uint localInvocationIndex: SV_GroupIndex,
    uniform float minLogLuminance,
    uniform float logLuminanceRange,
    uniform float adaptationUpRate,
    uniform float adaptationDownRate,
    uniform float evCompensation,
    uniform float histogramMin,
    uniform float histogramMax,
    uniform uint pixelCount,
    uniform ParameterBlock<ExposureFromHistogramResources> resources) {

    uint totalCount = 0;
    uint totalCountWeighted = 0;

    [unroll]
    for (uint i = 0; i < LUMINANCE_HISTOGRAM_BINS; i++) {
        totalCount += resources.bins[i];
        totalCountWeighted += resources.bins[i] * i;
    }

    const uint countMin = uint(histogramMin * totalCount);
    const uint countMax = uint(histogramMax * totalCount);
    totalCount = 0;
    totalCountWeighted = 0;
    uint countWithSkipped = 0;
    uint firstBin = ~0u;
    uint lastBin = 0u;
    for (uint i = 0; i < LUMINANCE_HISTOGRAM_BINS; i++) {
        const uint minCount = countWithSkipped;
        const uint maxCount = countWithSkipped + resources.bins[i];
        countWithSkipped = maxCount;

        if (maxCount <= countMin || minCount >= countMax)
            continue;

        uint newCount = 0;
        if (minCount <= countMin) {
            newCount = (maxCount - countMin);
        } else if (maxCount >= countMax) {
            newCount = (countMax - minCount);
        } else {
            newCount = resources.bins[i];
        }

        totalCount += newCount;
        totalCountWeighted += newCount * i;

        firstBin = min(firstBin, i);
        lastBin = i;
    }

    const uint aboveZeroLuminancePixelCount = max(1u, totalCount);
    const float weightedAverageLog = (float(totalCountWeighted) / float(aboveZeroLuminancePixelCount));
    const float weightedAverage = exp2(binToLogLuminance(weightedAverageLog, minLogLuminance, logLuminanceRange));

    const float targetExposure = convertEV100ToExposure(computeEV100(weightedAverage) - evCompensation);
    const float currentExposure = resources.view[0].shading.exposure;
    // higher target exposure means darker scene
    const float rate = targetExposure > currentExposure ? adaptationDownRate : adaptationUpRate;
    const float exposure = 
        currentExposure + 
        (targetExposure - currentExposure) * (1.0f - exp2(-resources.view[0].dt * rate));

    resources.view[0].shading.exposure = exposure;
    resources.view[0].shading.exposureInverse = 1.0f / resources.view[0].shading.exposure;
    resources.view[0].shading.fixedExposureToExposureMultiplier =
        resources.view[0].shading.exposure * resources.view[0].shading.fixedExposureInverse;
    resources.view[0].shading.exposureToFixedExposureMultiplier =
        1.0f / resources.view[0].shading.fixedExposureToExposureMultiplier;

    resources.output[0].currentEV = fromExposureToEV100(currentExposure);
    resources.output[0].targetCompensatedEV = fromExposureToEV100(targetExposure);
    resources.output[0].targetEV = resources.output[0].targetCompensatedEV + evCompensation;
    resources.output[0].firstAccountedEV = fromLogLuminanceToEV100(binToLogLuminance((float)firstBin,
        minLogLuminance, logLuminanceRange));
    resources.output[0].firstNotAccountedEV = fromLogLuminanceToEV100(binToLogLuminance((float)lastBin + 1.0f,
        minLogLuminance, logLuminanceRange));
}

struct VisualizeLuminanceHistogramResources {
    StructuredBuffer<uint> bins;
    StructuredBuffer<ExposureFromHistogramOutput> histogramOutput;
    RWTexture2D color;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void visualizeLuminanceHistogram(uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform float minLogLuminance,
    uniform float logLuminanceRange,
    uniform float evCompensation,
    uniform uint pixelCount,
    uniform ParameterBlock<VisualizeLuminanceHistogramResources> resources) {

    float2 imageSize;
    resources.color.GetDimensions(imageSize.x, imageSize.y);
    float2 imageSizeInverse = 1.0f / imageSize;

    if (dispatchThreadID.x >= imageSize.x || dispatchThreadID.y >= imageSize.y)
        return;

    const float2 uv = float2(dispatchThreadID + 0.5f) * imageSizeInverse;

    float4 color = plotHistogram(uv, minLogLuminance, logLuminanceRange, pixelCount, resources);
    resources.color[dispatchThreadID] = color;
}

static const float4 HISTOGRAM_DISCARD_LOW_COLOR = float4(0.18f, 0.18, 0.4f, 1.0f);
static const float4 HISTOGRAM_DISCARD_HIGH_COLOR = float4(0.4f, 0.18, 0.18f, 1.0f);
static const float4 HISTOGRAM_BACKGROUND_COLOR = float4(0.05f, 0.05f, 0.05f, 0.7f);
static const float4 HISTOGRAM_FOREGROUND_COLOR = float4(0.9f, 0.9f, 0.9f, 1.0f);
static const float4 HISTOGRAM_CURRENT_EV_COLOR = float4(0.0f, 0.0f, 0.0f, 1.0f);
static const float4 HISTOGRAM_TARGET_EV_COLOR = float4(0.2f, 0.9f, 0.9f, 1.0f);
static const float4 HISTOGRAM_COMPENSATED_EV_COLOR = float4(0.8f, 0.8f, 0.2f, 1.0f);

[shader("compute")]
[numthreads(8, 8, 1)]
void visualizeLuminanceHistogramOverlay(uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform float minLogLuminance,
    uniform float logLuminanceRange,
    uniform float evCompensation,
    uniform uint pixelCount,
    uniform ParameterBlock<VisualizeLuminanceHistogramResources> resources) {

    float2 imageSize;
    resources.color.GetDimensions(imageSize.x, imageSize.y);
    float2 imageSizeInverse = 1.0f / imageSize;

    if (dispatchThreadID.x >= imageSize.x || dispatchThreadID.y >= imageSize.y)
        return;

    const float2 uv = float2(dispatchThreadID + 0.5f) * imageSizeInverse;
    const float2 plotUvRangeMin = float2(0.1f, 0.7f);
    const float2 plotUvRangeMax = float2(0.9f, 0.95f);

    const float2 plotUv = (uv - plotUvRangeMin) / (plotUvRangeMax - plotUvRangeMin);
    float4 histogramPlot = float4(0.0f);
    if (all(plotUv > 0.0f) && all(plotUv < 1.0f))
        histogramPlot += plotHistogram(plotUv, minLogLuminance, logLuminanceRange, pixelCount, resources);
    float4 sceneColor = resources.color[dispatchThreadID];
    const float luminance = color.luminance(sceneColor.rgb / 
        convertEV100ToExposure(resources.histogramOutput[0].currentEV));
    const float ev100 = fromLuminanceToEV100(luminance);
    if (ev100 >= resources.histogramOutput[0].firstNotAccountedEV)
        sceneColor = HISTOGRAM_DISCARD_HIGH_COLOR;
    else if (ev100 < resources.histogramOutput[0].firstAccountedEV)
        sceneColor = HISTOGRAM_DISCARD_LOW_COLOR;

    const float4 blended = float4(sceneColor.rgb * (1.0f - histogramPlot.a) + histogramPlot.rgb * histogramPlot.a, 1.0f);
    resources.color[dispatchThreadID] = blended;
}

float4 plotHistogram(
    float2 uv, float minLogLuminance, float logLuminanceRange, uint pixelCount,
    ParameterBlock<VisualizeLuminanceHistogramResources> resources) {

    const uint binIndex = uint(uv.x * LUMINANCE_HISTOGRAM_BINS);
    // for better visualization, we take the sqrt of the in-bin-count, otherwise, the bars will be too squashed
    const float barHeight = sqrt((float)resources.bins[binIndex] / (float)pixelCount);
    const float ev = fromLogLuminanceToEV100(binToLogLuminance((float)binIndex, minLogLuminance, logLuminanceRange));
    const float barEvWidth = 
        fromLogLuminanceToEV100(binToLogLuminance((float)binIndex + 1, minLogLuminance, logLuminanceRange)) - ev;

    float4 color;
    if (abs(ev - resources.histogramOutput[0].currentEV) < barEvWidth * 0.5f && uv.y > 0.3f) {
        color = HISTOGRAM_CURRENT_EV_COLOR;
    } else if (abs(ev - resources.histogramOutput[0].targetEV) < barEvWidth * 0.5f && uv.y > 0.3f) {
        color = HISTOGRAM_TARGET_EV_COLOR;
    } else if (abs(ev - resources.histogramOutput[0].targetCompensatedEV) < barEvWidth * 0.5f && uv.y > 0.1f) {
        color = HISTOGRAM_COMPENSATED_EV_COLOR;
    } else if (1.0f - uv.y > barHeight) {
        color = HISTOGRAM_BACKGROUND_COLOR;
    } else {
        if (ev >= resources.histogramOutput[0].firstNotAccountedEV)
            color = HISTOGRAM_DISCARD_HIGH_COLOR;
        else if (ev < resources.histogramOutput[0].firstAccountedEV)
            color = HISTOGRAM_DISCARD_LOW_COLOR;
        else 
            color = HISTOGRAM_FOREGROUND_COLOR;
    }

    
    return color;
}

float fromLuminanceToEV100(float luminance) {
    return fromLogLuminanceToEV100(log2(luminance));
}

float fromLogLuminanceToEV100(float logLuminance) {
    return logLuminance + 3.0f;
}

float fromEV100ToLuminance(float EV) {
    return exp2(fromEV100ToLogLuminance(EV));
}

float fromEV100ToLogLuminance(float EV) {
    return EV - 3.0f;
}

float fromExposureToEV100(float exposure) {
    const float lum = 1.0f / exposure;

    return log2(lum / 1.2f);
}

float binToLogLuminance(float bin, float minLogLuminance, float logLuminanceRange) {
    return bin / (LUMINANCE_HISTOGRAM_BINS - 1) * logLuminanceRange + minLogLuminance;
}