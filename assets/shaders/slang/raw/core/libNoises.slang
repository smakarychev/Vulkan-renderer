implementing lib;

// https://github.com/BrianSharpe/GPU-Noise-Lib/blob/master/gpuNoiseLib.glsl

namespace noise
{
internal float3 interpolationC2(float3 x)
{
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

internal void perlinHash(float3 gridcell, float s, bool tile,
    out float4 lowzHash0,
    out float4 lowzHash1,
    out float4 lowzHash2,
    out float4 highzHash0,
    out float4 highzHash1,
    out float4 highzHash2)
{
    const float2 OFFSET = float2(50.0f, 161.0f);
    const float DOMAIN = 69.0f;
    const float3 SOMELARGEFLOATS = float3(635.298681f, 682.357502f, 668.926525f);
    const float3 ZINC = float3(48.500388f, 65.294118f, 63.934599f);

    gridcell.xyz = gridcell.xyz - floor(gridcell.xyz * (1.0f / DOMAIN)) * DOMAIN;
    float d = DOMAIN - 1.5f;
    float3 gridcellInc1 = step(gridcell, float3(d, d, d)) * (gridcell + 1.0f);

    gridcellInc1 = tile ? fmod(gridcellInc1, s) : gridcellInc1;

    float4 p = float4(gridcell.xy, gridcellInc1.xy) + OFFSET.xyxy;
    p *= p;
    p = p.xzxz * p.yyww;
    float3 lowzMod = float3(1.0f / (SOMELARGEFLOATS.xyz + gridcell.zzz * ZINC.xyz));
    float3 highzMod = float3(1.0f / (SOMELARGEFLOATS.xyz + gridcellInc1.zzz * ZINC.xyz));
    lowzHash0 = fract(p * lowzMod.xxxx);
    highzHash0 = fract(p * highzMod.xxxx);
    lowzHash1 = fract(p * lowzMod.yyyy);
    highzHash1 = fract(p * highzMod.yyyy);
    lowzHash2 = fract(p * lowzMod.zzzz);
    highzHash2 = fract(p * highzMod.zzzz);
}

public float perlin(float3 p, float s, bool tile)
{
    p *= s;

    float3 pi = floor(p);
    float3 pi2 = floor(p);
    float3 pf = p - pi;
    float3 pfMin1 = pf - 1.0f;

    float4 hashx0, hashy0, hashz0, hashx1, hashy1, hashz1;
    perlinHash(pi2, s, tile, hashx0, hashy0, hashz0, hashx1, hashy1, hashz1);

    float4 gradX0 = hashx0 - 0.49999f;
    float4 gradY0 = hashy0 - 0.49999f;
    float4 gradZ0 = hashz0 - 0.49999f;
    float4 gradX1 = hashx1 - 0.49999f;
    float4 gradY1 = hashy1 - 0.49999f;
    float4 gradZ1 = hashz1 - 0.49999f;
    float4 gradResults0 = rsqrt(gradX0 * gradX0 + gradY0 * gradY0 + gradZ0 * gradZ0) * (float2(pf.x, pfMin1.x).xyxy * gradX0 + float2(pf.y, pfMin1.y).xxyy * gradY0 + pf.zzzz * gradZ0);
    float4 gradResults1 = rsqrt(gradX1 * gradX1 + gradY1 * gradY1 + gradZ1 * gradZ1) * (float2(pf.x, pfMin1.x).xyxy * gradX1 + float2(pf.y, pfMin1.y).xxyy * gradY1 + pfMin1.zzzz * gradZ1);

    float3 blend = interpolationC2(pf);
    float4 res0 = lerp(gradResults0, gradResults1, blend.z);
    float4 blend2 = float4(blend.xy, float2(1.0f - blend.xy));
    float final = dot(res0, blend2.zxzx * blend2.wwyy);
    final *= 1.0f / sqrt(0.75f);
    return ((final * 1.5f) + 1.0f) * 0.5f;
}

internal float perlinN<let N : uint>(float3 p, float s, bool tile) {
    float3 xyz = p;
    float f = 1.0f;
    float a = 1.0f;

    float perlinValue = 0.0f;
    [unroll]
    for (uint i = 0; i < N; i++) {
        perlinValue += a * perlin(xyz, s * f, tile).r;
        a *= 0.5f;
        f *= 2.0f;
    }

    return perlinValue;
}

public float perlin3(float3 p, float s, bool tile)
{
    return perlinN<3>(p, s, tile);
}

public float perlin5(float3 p, float s, bool tile)
{
    return perlinN<5>(p, s, tile);
}

public float perlin7(float3 p, float s, bool tile)
{
    return perlinN<7>(p, s, tile);
}

internal float3 voronoiHash(float3 x, float s)
{
    x = fmod(x, s);
    x = float3(dot(x, float3(127.1f, 311.7f, 74.7f)),
    dot(x, float3(269.5f, 183.3f, 246.1f)),
    dot(x, float3(113.5f, 271.9f, 124.6f)));

    return fract(sin(x) * 43758.5453123f);
}

public float3 voronoi(in float3 x, float s, float seed, bool inverted)
{
    x *= s;
    x += 0.5;
    float3 p = floor(x);
    float3 f = fract(x);

    float id = 0.0;
    float2 res = float2(1.0, 1.0);
    for (int k = -1; k <= 1; k++)
    {
        for (int j = -1; j <= 1; j++)
        {
            for (int i = -1; i <= 1; i++)
            {
                float3 b = float3(i, j, k);
                float3 r = float3(b) - f + voronoiHash(p + b + seed * 10.0f, s);
                float d = dot(r, r);

                if (d < res.x)
                {
                    id = dot(p + b, float3(1.0f, 57.0f, 113.0f));
                    res = float2(d, res.x);
                }
                else if (d < res.y)
                {
                    res.y = d;
                }
            }
        }
    }

    float2 result = res;
    id = abs(id);

    if (inverted)
        return float3(1.0f - result, id);
    else
        return float3(result, id);
}

public float worley(float3 p, float s, float seed) {
    return voronoi(p, s, seed, true).r;
}

public float worley(float3 p, float s) {
    return voronoi(p, s, 0.0f, true).r;
}

public float worley2(float3 p, float s, float seed)
{
    float3 xyz = p;

    float worleyValue1 = voronoi(xyz, 1.0f * s, seed, true).r;
    float worleyValue2 = voronoi(xyz, 2.0f * s, seed, false).r;

    worleyValue1 = clamp(worleyValue1, 0.0f, 1.0f);
    worleyValue2 = clamp(worleyValue2, 0.0f, 1.0f);

    return worleyValue1 * 0.625f - worleyValue2 * 0.25f;
}

public float worley2(float3 p, float s)
{
    return worley2(p, s, 0.0f);
}

public float worley3(float3 p, float s, float seed)
{
    float3 xyz = p;

    float worleyValue1 = voronoi(xyz, 2.0f * s, seed, true).r;
    float worleyValue2 = voronoi(xyz, 8.0f * s, seed, false).r;
    float worleyValue3 = voronoi(xyz, 14.0f * s, seed, false).r;

    worleyValue1 = clamp(worleyValue1, 0.0f, 1.0f);
    worleyValue2 = clamp(worleyValue2, 0.0f, 1.0f);
    worleyValue3 = clamp(worleyValue3, 0.0f, 1.0f);

    return worleyValue1 * 0.625f - worleyValue2 * 0.25f - worleyValue3 * 0.125f;
}

public float worley3(float3 p, float s)
{
    return worley3(p, s, 0.0f);
}

public float alligator(float3 p, float s, float seed, bool inverted) {
    const float3 voronoi = voronoi(p, s, seed, true);
    if (!inverted)
        return 1 - voronoi.y - voronoi.x;

    return voronoi.x - voronoi.y;
}

public float alligator(float3 p, float s, float seed) {
    const float3 voronoi = voronoi(p, s, seed, true);

    return voronoi.x - voronoi.y;
}

public float alligator(float3 p, float s) {
    return alligator(p, s, 0, true);
}

public float alligator2(float3 p, float s, float seed)
{
    float3 xyz = p;

    float alligatorValue1 = alligator(xyz, 1.0f * s, seed, true);
    float alligatorValue2 = alligator(xyz, 2.0f * s, seed, true);

    return alligatorValue1 + alligatorValue2 * 0.5f;
}

public float alligator2(float3 p, float s)
{
    return alligator2(p, s, 0.0f);
}

public float alligator3(float3 p, float s, float seed)
{
    float3 xyz = p;

    float alligatorValue1 = alligator(xyz, 1.0f * s, seed, true);
    float alligatorValue2 = alligator(xyz, 2.0f * s, seed, true);
    float alligatorValue3 = alligator(xyz, 4.0f * s, seed, true);

    return alligatorValue1 + alligatorValue2 * 0.5f + alligatorValue3 * 0.25f;
}

public float alligator3(float3 p, float s)
{
    return alligator3(p, s, 0.0f);
}

public float3 curl(float3 uv, float frequency) {
    const float epsilon = 0.05f;
    float noise1, noise2, a, b;
    float3 c;

    const float3 pos = uv * frequency;
    noise1 = perlin5(pos.xyz + float3(0.0f, epsilon, 0.0), 1.0f, false);
    noise2 = perlin5(pos.xyz + float3(0.0f, -epsilon, 0.0), 1.0f, false);
    a = (noise1 - noise2) / (2 * epsilon);
    noise1 = perlin5(pos.xyz + float3(0.0f, 0.0f, epsilon), 1.0f, false);
    noise2 = perlin5(pos.xyz + float3(0.0f, 0.0f, -epsilon), 1.0f, false);
    b = (noise1 - noise2) / (2 * epsilon);

    c.x = a - b;

    noise1 = perlin5(pos.xyz + float3(0.0f, 0.0f,  epsilon), 1.0f, false);
    noise2 = perlin5(pos.xyz + float3(0.0f, 0.0f, -epsilon), 1.0f, false);
    a = (noise1 - noise2) / (2 * epsilon);
    noise1 = perlin5(pos.xyz + float3( epsilon, 0.0f, 0.0), 1.0f, false);
    noise2 = perlin5(pos.xyz + float3(-epsilon, 0.0f, 0.0), 1.0f, false);
    b = (noise1 - noise2) / (2 * epsilon);

    c.y = a - b;

    noise1 = perlin5(pos.xyz + float3( epsilon, 0.0f, 0.0), 1.0f, false);
    noise2 = perlin5(pos.xyz + float3(-epsilon, 0.0f, 0.0), 1.0f, false);
    a = (noise1 - noise2) / (2 * epsilon);
    noise1 = perlin5(pos.xyz + float3(0.0f,  epsilon, 0.0), 1.0f, false);
    noise2 = perlin5(pos.xyz + float3(0.0f, -epsilon, 0.0), 1.0f, false);
    b = (noise1 - noise2) / (2 * epsilon);

    c.z = a - b;

    const float remapLow = -0.5;
    const float remapHigh = 3.0;
    float3 noise = (c - remapLow) / (remapHigh - remapLow);

    return noise;
}

public float curlyAlligator(float3 p, float freq) {
    const float alligator = 1.0 - alligator3(p + curl(p, freq) / 20, freq);

    return alligator;
}

public float blueSample128(Texture2D blueNoise, uint2 coords, float frameNumber) {
    return fract(blueNoise[coords.xy % 128].r + frameNumber * PHI_GOLDEN);
}

}