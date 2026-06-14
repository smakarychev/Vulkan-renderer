module tonemapping;

import "core/lib";

static const uint TONEMAPPING_REINHARD = 0;
static const uint TONEMAPPING_REINHARD_LUMINANCE = 1;
static const uint TONEMAPPING_HABLE = 2;
static const uint TONEMAPPING_PBR_NEUTRAL = 3;
static const uint TONEMAPPING_AGX = 4;

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
    else
        tonemapped = float4(agx(color.rgb), 1.0f);

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
