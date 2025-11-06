module fxaa;

import "core/attributes";

struct FxaaSamplers {
    [ImmutableSampler(SamplerFlags.Linear | SamplerFlags.ClampEdge)]
    SamplerState sampler;
}

struct FxaaResources {
    Texture2D color;
    RWTexture2D<float4> antialiased;
}

ParameterBlock<FxaaSamplers> samplers;
ParameterBlock<FxaaResources> resources;

static const float EDGE_THRESHOLD_MIN = 0.0312f;
static const float EDGE_THRESHOLD_MAX = 0.125f;
static const float SUBPIXEL_QUALITY = 0.75f;
static const uint MAX_STEPS = 12;
static const float STEP_SIZES[MAX_STEPS] =
    float[MAX_STEPS](1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 2.0f, 2.0f, 2.0f, 2.0f, 4.0f, 8.0f);
static const float LAST_STEP_SIZE = 8.0f;

float3 getColor(float2 uv) {
    return resources.color.SampleLevel(samplers.sampler, uv, 0).rgb;
}

float luminance(float2 uv) {
    return dot(getColor(uv), float3(0.2126729f,  0.7151522f, 0.0721750f));
}

struct Luma {
    float m, l, r, b, t, bl, br, tl, tr; 
    float min, max, contrast;  
}

Luma calculateLuma(float2 uv, float2 delta) {
    Luma luma;
    luma.m  = luminance(uv);
    luma.l  = luminance(uv + float2(-delta.x, 0.0f));
    luma.r  = luminance(uv + float2( delta.x, 0.0f));
    luma.b  = luminance(uv + float2(0.0f, -delta.y));
    luma.t  = luminance(uv + float2(0.0f,  delta.y));
    luma.bl = luminance(uv + float2(-delta.x, -delta.y));
    luma.br = luminance(uv + float2( delta.x, -delta.y));
    luma.tl = luminance(uv + float2(-delta.x,  delta.y));
    luma.tr = luminance(uv + float2( delta.x,  delta.y));
    luma.min = min(luma.m, min(min(luma.l, luma.r), min(luma.b, luma.t)));
    luma.max = max(luma.m, max(max(luma.l, luma.r), max(luma.b, luma.t)));
    luma.contrast = luma.max - luma.min;
    
    return luma;
}

float getSubpixelBlendFactor(Luma luma) {
    float factor = 2.0f * (luma.b + luma.t + luma.l + luma.r);
    factor += luma.bl + luma.br + luma.tl + luma.tr;
    factor *= 1.0f / 12.0f;
    factor = abs(factor - luma.m) / luma.contrast;
    factor = saturate(factor);
    factor = smoothstep(0.0f, 1.0f, factor);

    return factor * factor * SUBPIXEL_QUALITY;
}

bool isHorizontal(Luma luma) {
    const float horizontal = 
        2.0f * abs(luma.b + luma.t - 2.0f * luma.m) +
        abs(luma.bl + luma.tl - 2.0f * luma.l) +
        abs(luma.br + luma.tr - 2.0f * luma.r);
    const float vertical =
        2.0f * abs(luma.l + luma.r - 2.0f * luma.m) +
        abs(luma.bl + luma.br - 2.0f * luma.b) +
        abs(luma.tl + luma.tr - 2.0f * luma.t);
    
    return horizontal >= vertical;
}

struct Edge {
    bool isHorizontal;
    float step;
    float gradient;
    float otherLuma;
}

Edge calculateEdge(Luma luma, float2 delta) {
    Edge edge;
    edge.isHorizontal = isHorizontal(luma);
    float lumaPositive, lumaNegative;
    if (edge.isHorizontal) {
        edge.step = delta.y;
        lumaPositive = luma.t;
        lumaNegative = luma.b;
    } else {
        edge.step = delta.x;
        lumaPositive = luma.r;
        lumaNegative = luma.l;
    }
    const float gradientPositive = abs(lumaPositive - luma.m);
    const float gradientNegative = abs(lumaNegative - luma.m);
    
    if (gradientPositive < gradientNegative) {
        edge.step = -edge.step;
        edge.gradient = gradientNegative;
        edge.otherLuma = lumaNegative;
    } else {
        edge.gradient = gradientNegative;
        edge.otherLuma = lumaPositive;
    }
    
    return edge;
}

float getEdgeBlendFactor(Luma luma, Edge edge, float2 uv, float2 delta) {
    float2 edgeUv = uv;
    float2 edgeDelta = float2(0.0f);
    if (edge.isHorizontal) {
        edgeUv.y += 0.5f * edge.step;
        edgeDelta.x = delta.x;
    } else {
        edgeUv.x += 0.5f * edge.step;
        edgeDelta.y = delta.y;
    }
    
    const float edgeLuma = (edge.otherLuma + luma.m) * 0.5f;
    const float gradientThreshold = edgeLuma * 0.25f;
    
    float2 uvPositive = edgeUv + edgeDelta;
    float2 uvNegative = edgeUv - edgeDelta;
    float lumaDeltaPositive = luminance(uvPositive) - edgeLuma;
    float lumaDeltaNegative = luminance(uvNegative) - edgeLuma;
    bool positiveEnd = abs(lumaDeltaPositive) > gradientThreshold;
    bool negativeEnd = abs(lumaDeltaNegative) > gradientThreshold;
    [unroll]
    for (uint i = 0; i < MAX_STEPS; i++) {
        if (!positiveEnd) {
            uvPositive += edgeDelta * STEP_SIZES[i];
            lumaDeltaPositive = luminance(uvPositive - edgeLuma);
            positiveEnd = abs(lumaDeltaPositive) > gradientThreshold;    
        }
        if (!negativeEnd) {
            uvNegative -= edgeDelta * STEP_SIZES[i];
            lumaDeltaNegative = luminance(uvNegative - edgeLuma);
            negativeEnd = abs(lumaDeltaNegative) > gradientThreshold;
        }
        
        if (positiveEnd && negativeEnd)
            break;
    }
    if (!positiveEnd)
        uvPositive += edgeDelta * LAST_STEP_SIZE;
    if (!negativeEnd)
        uvNegative -= edgeDelta * LAST_STEP_SIZE;
    
    float distancePositive;
    float distanceNegative;
    if (edge.isHorizontal) {
        distancePositive = uvPositive.x - uv.x;
        distanceNegative = uv.x - uvNegative.x;
    } else {
        distancePositive = uvPositive.y - uv.y;
        distanceNegative = uv.y - uvNegative.y;
    }
    
    float distance;
    bool sign;
    if (distancePositive < distanceNegative) {
        distance = distancePositive;
        sign = lumaDeltaPositive > 0.0f;
    } else {
        distance = distanceNegative;
        sign = lumaDeltaNegative > 0.0f;
    }
    
    if (sign == (luma.m - edgeLuma >= 0.0f))
        return 0.0f;
  
    return 0.5f - distance / (distancePositive + distanceNegative);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint2 dispatchThreadID: SV_DispatchThreadID) {
    const uint2 coord = dispatchThreadID;
    uint2 textureSize;
    resources.color.GetDimensions(textureSize.x, textureSize.y);
    if (any(coord >= textureSize))
        return;

    const float2 textureSizeInv = 1.0f / float2(textureSize);
    const float2 uv = (float2(coord) + 0.5f) * textureSizeInv;
    const float2 delta = textureSizeInv;
    
    const Luma luma = calculateLuma(uv, delta);
    if (luma.contrast < max(EDGE_THRESHOLD_MIN, luma.max * EDGE_THRESHOLD_MAX)) {
        resources.antialiased[coord] = float4(getColor(uv), 1.0f);
        return;
    }
    
    const float subpixelBlenderFactor = getSubpixelBlendFactor(luma);
    const Edge edge = calculateEdge(luma, delta);
    const float edgeBlendFactor = getEdgeBlendFactor(luma, edge, uv, delta);
    const float blendFactor = max(subpixelBlenderFactor, edgeBlendFactor);
    
    float2 blendUv = uv;
    if (edge.isHorizontal)
        blendUv.y += blendFactor * edge.step;
    else
        blendUv.x += blendFactor * edge.step;
    
    resources.antialiased[coord] = float4(getColor(blendUv), 1.0f);
}