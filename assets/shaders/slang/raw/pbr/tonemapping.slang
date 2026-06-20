module tonemapping;

import "core/lib";

static const uint TONEMAPPING_REINHARD = 0;
static const uint TONEMAPPING_REINHARD_LUMINANCE = 1;
static const uint TONEMAPPING_HABLE = 2;
static const uint TONEMAPPING_PBR_NEUTRAL = 3;
static const uint TONEMAPPING_AGX = 4;
static const uint TONEMAPPING_GT7 = 5;

[SpecializationConstant] const uint TONEMAPPING_TYPE = TONEMAPPING_AGX;

struct Resources {
    RWTexture2D color;
}

ParameterBlock<Resources> resources;

[shader("compute")]
[numthreads(8, 8, 1)]
void tonemap(uint2 dispatchThreadID: SV_DispatchThreadID) {
    uint2 imageSize;
    resources.color.GetDimensions(imageSize.x, imageSize.y);

    if (dispatchThreadID.x >= imageSize.x || dispatchThreadID.y >= imageSize.y)
        return;

    const float4 color = resources.color[dispatchThreadID];

    float4 tonemapped;
    if (TONEMAPPING_TYPE == TONEMAPPING_REINHARD)
        tonemapped = float4(reinhardBasic(color.rgb), 1.0f);
    else if (TONEMAPPING_TYPE == TONEMAPPING_REINHARD_LUMINANCE)
        tonemapped = float4(reinhardLuminance(color.rgb), 1.0f);
    else if (TONEMAPPING_TYPE == TONEMAPPING_HABLE)
        tonemapped = float4(uncharted2Tonemap(color.rgb), 1.0f);
    else if (TONEMAPPING_TYPE == TONEMAPPING_PBR_NEUTRAL)
        tonemapped = float4(pbrNeutral(color.rgb), 1.0f);
    else if (TONEMAPPING_TYPE == TONEMAPPING_AGX)
        tonemapped = float4(agx(color.rgb), 1.0f);
    else
        tonemapped = float4(gt7WithGtCurve(color.rgb), 1.0f);

    resources.color[dispatchThreadID] = tonemapped;
}

float3 reinhardBasic(float3 c) {
    return c / (1.0f + c);
}
float3 reinhardExtended(float3 c, float w) {
    return c * (1.0f + c / (w * w)) / (1.0f + c);
}
float3 reinhardLuminance(float3 c) {
    return c / (1.0f + color.luminance(c));
}

float3 uncharted2TonemapHelper(float3 color) {
	const float A = 0.15f;
	const float B = 0.50f;
	const float C = 0.10f;
	const float D = 0.20f;
	const float E = 0.02f;
	const float F = 0.30f;
	const float W = 11.2f;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}
float3 uncharted2Tonemap(float3 color) {
    float3 outColor = uncharted2TonemapHelper(color * 2.45f);
    outColor = outColor * (1.0f / uncharted2TonemapHelper(float3(11.2f)));
    
    return outColor;
}

float3 pbrNeutral(float3 c) {
    // https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/

    const float f90 = 0.04f;
    const float Ks = 0.8f - f90;
    const float Kd = 0.15f;

    const float x = min(c.r, min(c.g, c.b));
    const float f = x < 2.0f * f90 ? x - 6.25f * x * x : f90;
    c -= f;

    const float p = max(c.r, max(c.g, c.b));
    if (p < Ks)
        return c;

    const float pn = 1.0f - sqr(1.0f - Ks) / (p + 1.0f - 2.0f * Ks);
    c *= pn / p;

    const float g = 1.0f / (Kd * (p - pn) + 1.0f);

    return lerp(pn * float3(1.0f), c, g);
}

float3 agx(float3 c) {
    // https://iolite-engine.com/blog_posts/minimal_agx_implementation

    const float3x3 agxMat = float3x3(
        0.842479062253094f, 0.0423282422610123f, 0.0423756549057051f,
        0.0784335999999992f, 0.878468636469772f, 0.0784336f,
        0.0792237451477643f, 0.0791661274605434f, 0.879142973793104f
    );
    const float3x3 agxMatInv = float3x3(
        1.19687900512017f, -0.0528968517574562f, -0.0529716355144438f,
        -0.0980208811401368f, 1.15190312990417f, -0.0980434501171241f,
        -0.0990297440797205f, -0.0989611768448433f, 1.15107367264116f
    );
    
    const float minEv = -12.4739311883324f;
    const float maxEv = 4.02606881166759f;

    // Input transform (inset)
    c = mul(c, agxMat);
    c = max(c, 1e-10f);
    
    c = clamp(log2(c), minEv, maxEv);
    c = (c - minEv) / (maxEv - minEv);
    c = agxDefaultContrastApprox(c);
    
    c = agxLook(c);

    c = mul(c, agxMatInv);
    
    // agx assumes linear-srgb colorspace as input
    // for whatever reason, after all the magic happened, we apparently end up in gamma-srgb colorspace
    // so we convert back to linear:
    c = pow(max(c, 0.0f), 2.2f);
    
    return c;
}
float3 agxDefaultContrastApprox(float3 c) {
  const float3 c2 = c * c;
  const float3 c4 = c2 * c2;
  
  return + 15.5f     * c4 * c2
         - 40.14f    * c4 * c
         + 31.96f    * c4
         - 6.868f    * c2 * c
         + 0.4298f   * c2
         + 0.1191f   * c
         - 0.00232f;
}
float3 agxLook(float3 c) {  
  // Default
  float3 offset = float3(0.0f);
  float3 slope = float3(1.0f);
  float3 power = float3(1.0f);
  float sat = 1.0f;
 
#if AGX_LOOK_PUNCHY
  // Punchy
  slope = float3(1.0f);
  power = float3(1.35f, 1.35f, 1.35f);
  sat = 1.4f;
#endif // AGX_LOOK_PUNCHY
  
  // ASC CDL
  c = pow(c * slope + offset, power);

  const float luminance = color.luminance(c);

  return luminance + sat * (c - luminance);
}

static const float GT7_TARGET_WHILE_LEVEL = 250.0f;

float3 gt7WithGtCurve(float3 c) {
    const float EXPOSURE_MULTIPLIER = 1.98f;
    
    const float blendRatio = 0.6f;
    const float fadeStart = 0.98f;
    const float fadeEnd = 1.16f;
    const float peakIntensity = gt7PhysicalToFramebufferLuminance(GT7_TARGET_WHILE_LEVEL);
    const float peakIntensityUcs = gt7RgbToUcs(peakIntensity).r;
    const float sdrCorrection = 1.0f / peakIntensity;
    
    c *= EXPOSURE_MULTIPLIER;
    const float3 ucs = gt7RgbToUcs(c);

    const float3 skewedRgb = gtEvaluateCurve(c, peakIntensity);
    const float3 skewedUcs = gt7RgbToUcs(skewedRgb);

    float chromaScale = 1.0f - smoothstep(fadeStart, fadeEnd, ucs.r / peakIntensityUcs);
    const float3 scaledUcs = float3(
        skewedUcs.r,
        ucs.g * chromaScale,
        ucs.b * chromaScale
    );
    const float3 scaledRgb = gt7UcsToRgb(scaledUcs);

    const float3 blend = lerp(skewedRgb, scaledRgb, blendRatio);

    return sdrCorrection * min(blend, peakIntensity);
}
float3 gt7EvaluateCurve(float3 c, float peakIntensity) {

    const float alpha = 0.25f;
    const float midPoint = 0.538f;
    const float linearSection = 0.444f;
    const float toeStrength = 1.280f;

    const float k = (linearSection - 1.0f) / (alpha - 1.0f);
    const float kA = peakIntensity * linearSection + peakIntensity * k;
    const float kB = -peakIntensity * k * exp(linearSection / k);
    const float kC = -1.0f / (k * peakIntensity);

    c = max(c, 0.0f);

    float3 weightLinear = smoothstep(0.0f, midPoint, c);
    float3 weightToe = 1.0f - weightLinear;

    float3 shoulder = kA + kB * exp(c * kC);

    [unroll]
    for (uint i = 0; i < 3; i++)
    {
        if (c[i] < linearSection * peakIntensity)
        {
            float toeMapped = midPoint * pow(c[i] / midPoint, toeStrength);
            c[i] = weightToe[i] * toeMapped + weightLinear[i] * c[i];
        }
        else
        {
            c[i] = shoulder[i];
        }
    }

    return c;
}
float3 gtEvaluateCurve(float3 c, float peakIntensity) {
    const float alpha = 1.0f;
    const float midPoint = 0.22f;
    const float linearSection = 0.4f;
    const float toeStrength = 1.33f;

    const float l0 = (peakIntensity - midPoint) * linearSection / alpha;
    const float s0 = midPoint + l0;
    const float c2 = alpha * peakIntensity / (peakIntensity - s0);
    
    c = max(c, 0.0f);

    const float3 weightToe = 1.0f - smoothstep(0.0f, midPoint, c);
    const float3 weightShoulder = step(s0, c);
    const float3 weightLinear = 1.0f - weightToe - weightShoulder;

    const float3 toe = midPoint * pow((c / midPoint), toeStrength);
    const float3 shoulder = peakIntensity - (peakIntensity - s0) * exp(-(c2 * (c - s0)) / peakIntensity);
    const float3 linear = midPoint + alpha * (c - midPoint);

    return (weightToe * toe + weightShoulder * shoulder + weightLinear * linear);
}
float3 gt7RgbToUcs(float3 c) {
    const float l = (c.r * 1688.0f + c.g * 2146.0f + c.b * 262.0f) / 4096.0f;
    const float m = (c.r * 683.0f  + c.g * 2951.0f + c.b * 462.0f) / 4096.0f;
    const float s = (c.r * 99.0f   + c.g * 309.0f  + c.b * 3688.0f) / 4096.0f;

    const float lPQ = gt7InverseEotfSt2084(l);
    const float mPQ = gt7InverseEotfSt2084(m);
    const float sPQ = gt7InverseEotfSt2084(s);

    return float3(
        (2048.0f * lPQ + 2048.0f * mPQ) / 4096.0f,
        (6610.0f * lPQ - 13613.0f * mPQ + 7003.0f * sPQ) / 4096.0f,
        (17933.0f * lPQ - 17390.0f * mPQ - 543.0f * sPQ) / 4096.0f
    );
}
float3 gt7UcsToRgb(float3 c) {
    const float l = c.r + 0.00860904f * c.g + 0.11103f  * c.b;
    const float m = c.r - 0.00860904f * c.g - 0.11103f  * c.b;
    const float s = c.r + 0.560031f   * c.g - 0.320627f * c.b;

    const float lLin = gt7EotfSt2084(l);
    const float mLin = gt7EotfSt2084(m);
    const float sLin = gt7EotfSt2084(s);

    return float3(
        max(3.43661f * lLin - 2.50645f * mLin + 0.0698454f * sLin, 0.0f),
        max(-0.79133f * lLin + 1.9836f * mLin - 0.192271f * sLin, 0.0f),
        max(-0.0259499f * lLin - 0.0989137f * mLin + 1.12486f * sLin, 0.0f)
    );
}
float gt7EotfSt2084(float n)
{
    n = saturate(n);

    const float m1  = 0.1593017578125f;
    const float m2  = 78.84375f;
    const float c1  = 0.8359375f;
    const float c2  = 18.8515625f;
    const float c3  = 18.6875f;
    const float pqC = 10000.0f;

    const float np = pow(n, 1.0f / m2);
    float l  = np - c1;

    if (l < 0.0f)
        l = 0.0f;

    l = l / (c2 - c3 * np);
    l = pow(l, 1.0f / m1);

    return gt7PhysicalToFramebufferLuminance(l * pqC);
}

float gt7InverseEotfSt2084(float v)
{
    const float m1  = 0.1593017578125f;
    const float m2  = 78.84375f;
    const float c1  = 0.8359375f;
    const float c2  = 18.8515625f;
    const float c3  = 18.6875f;
    const float pqC = 10000.0f;

    const float physical = gt7FramebufferToPhysicalLuminance(v);
    const float y = physical / pqC;
    const float ym = pow(y, m1);

    return exp2(m2 * (log2(c1 + c2 * ym) - log2(1.0f + c3 * ym)));
}

static const float GT7_REFERENCE_LUMINANCE = 100.0f;

float gt7FramebufferToPhysicalLuminance(float luminance) {
    return luminance * GT7_REFERENCE_LUMINANCE;
}
float gt7PhysicalToFramebufferLuminance(float luminance) {
    return luminance / GT7_REFERENCE_LUMINANCE;
}