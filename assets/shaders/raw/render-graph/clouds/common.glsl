float remap(float val, float omin, float omax, float nmin, float nmax) {
    return nmin + (val - omin) / (omax - omin) * (nmax - nmin);
}

float remap_clamp(float val, float omin, float omax, float nmin, float nmax) {
    return clamp(remap(val, omin, omax, nmin, nmax), nmin, nmax);
}

float remap_01(float val, float nmin, float nmax) {
    return clamp((val - nmin) / (nmax - nmin), 0.0f, 1.0f);
}

vec3 remap_01_noclamp(vec3 val, float nmin, float nmax) {
    return (val - nmin) / (nmax - nmin);
}

vec3 encode_curl(vec3 noise) {
    return (noise + 1.0f) * 0.5f;
}

vec3 decode_curl(vec3 noise) {
    return (noise - 0.5f) * 2.0f;
}

// modified from: https://github.com/greje656/clouds


// https://github.com/BrianSharpe/GPU-Noise-Lib/blob/master/gpu_noise_lib.glsl


vec3 interpolation_C2(vec3 x)
{
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

void perlin_hash(vec3 gridcell, float s, bool tile,
    out vec4 lowz_hash_0,
    out vec4 lowz_hash_1,
    out vec4 lowz_hash_2,
    out vec4 highz_hash_0,
    out vec4 highz_hash_1,
    out vec4 highz_hash_2)
{
    const vec2 OFFSET = vec2(50.0f, 161.0f);
    const float DOMAIN = 69.0f;
    const vec3 SOMELARGEFLOATS = vec3(635.298681f, 682.357502f, 668.926525f);
    const vec3 ZINC = vec3(48.500388f, 65.294118f, 63.934599f);

    gridcell.xyz = gridcell.xyz - floor(gridcell.xyz * (1.0f / DOMAIN)) * DOMAIN;
    float d = DOMAIN - 1.5f;
    vec3 gridcell_inc1 = step(gridcell, vec3(d, d, d)) * (gridcell + 1.0f);

    gridcell_inc1 = tile ? mod(gridcell_inc1, s) : gridcell_inc1;

    vec4 P = vec4(gridcell.xy, gridcell_inc1.xy) + OFFSET.xyxy;
    P *= P;
    P = P.xzxz * P.yyww;
    vec3 lowz_mod = vec3(1.0f / (SOMELARGEFLOATS.xyz + gridcell.zzz * ZINC.xyz));
    vec3 highz_mod = vec3(1.0f / (SOMELARGEFLOATS.xyz + gridcell_inc1.zzz * ZINC.xyz));
    lowz_hash_0 = fract(P * lowz_mod.xxxx);
    highz_hash_0 = fract(P * highz_mod.xxxx);
    lowz_hash_1 = fract(P * lowz_mod.yyyy);
    highz_hash_1 = fract(P * highz_mod.yyyy);
    lowz_hash_2 = fract(P * lowz_mod.zzzz);
    highz_hash_2 = fract(P * highz_mod.zzzz);
}

float perlin(vec3 P, float s, bool tile)
{
    P *= s;

    vec3 Pi = floor(P);
    vec3 Pi2 = floor(P);
    vec3 Pf = P - Pi;
    vec3 Pf_min1 = Pf - 1.0f;

    vec4 hashx0, hashy0, hashz0, hashx1, hashy1, hashz1;
    perlin_hash(Pi2, s, tile, hashx0, hashy0, hashz0, hashx1, hashy1, hashz1);

    vec4 grad_x0 = hashx0 - 0.49999f;
    vec4 grad_y0 = hashy0 - 0.49999f;
    vec4 grad_z0 = hashz0 - 0.49999f;
    vec4 grad_x1 = hashx1 - 0.49999f;
    vec4 grad_y1 = hashy1 - 0.49999f;
    vec4 grad_z1 = hashz1 - 0.49999f;
    vec4 grad_results_0 = inversesqrt(grad_x0 * grad_x0 + grad_y0 * grad_y0 + grad_z0 * grad_z0) * (vec2(Pf.x, Pf_min1.x).xyxy * grad_x0 + vec2(Pf.y, Pf_min1.y).xxyy * grad_y0 + Pf.zzzz * grad_z0);
    vec4 grad_results_1 = inversesqrt(grad_x1 * grad_x1 + grad_y1 * grad_y1 + grad_z1 * grad_z1) * (vec2(Pf.x, Pf_min1.x).xyxy * grad_x1 + vec2(Pf.y, Pf_min1.y).xxyy * grad_y1 + Pf_min1.zzzz * grad_z1);

    vec3 blend = interpolation_C2(Pf);
    vec4 res0 = mix(grad_results_0, grad_results_1, blend.z);
    vec4 blend2 = vec4(blend.xy, vec2(1.0f - blend.xy));
    float final = dot(res0, blend2.zxzx * blend2.wwyy);
    final *= 1.0f / sqrt(0.75f);
    return ((final * 1.5f) + 1.0f) * 0.5f;
}

float perlin_5_octaves(vec3 p, bool tile)
{
    vec3 xyz = p;
    float amplitude_factor = 0.5f;
    float frequency_factor = 2.0f;

    float a = 1.0f;
    float perlin_value = 0.0f;
    perlin_value += a * perlin(xyz, 1.0f, tile).r;
    a *= amplitude_factor;
    xyz *= (frequency_factor + 0.02f);
    perlin_value += a * perlin(xyz, 1.0f, tile).r;
    a *= amplitude_factor;
    xyz *= (frequency_factor + 0.03f);
    perlin_value += a * perlin(xyz, 1.0f, tile).r;
    a *= amplitude_factor;
    xyz *= (frequency_factor + 0.01f);
    perlin_value += a * perlin(xyz, 1.0f, tile).r;
    a *= amplitude_factor;
    xyz *= (frequency_factor + 0.01f);
    perlin_value += a * perlin(xyz, 1.0f, tile).r;

    return perlin_value;
}

float perlin_5_octaves(vec3 p, float s, bool tile)
{
    vec3 xyz = p;
    float f = 1.0f;
    float a = 1.0f;

    float perlin_value = 0.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;
    a *= 0.5f;
    f *= 2.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;
    a *= 0.5f;
    f *= 2.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;
    a *= 0.5f;
    f *= 2.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;
    a *= 0.5f;
    f *= 2.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;

    return perlin_value;
}

float perlin_3_octaves(vec3 p, float s, bool tile)
{
    vec3 xyz = p;
    float f = 1.0f;
    float a = 1.0f;

    float perlin_value = 0.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;
    a *= 0.5f;
    f *= 2.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;
    a *= 0.5f;
    f *= 2.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;

    return perlin_value;
}

float perlin_7_octaves(vec3 p, float s, bool tile)
{
    vec3 xyz = p;
    float f = 1.0f;
    float a = 1.0f;

    float perlin_value = 0.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;
    a *= 0.5f;
    f *= 2.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;
    a *= 0.5f;
    f *= 2.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;
    a *= 0.5f;
    f *= 2.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;
    a *= 0.5f;
    f *= 2.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;
    a *= 0.5f;
    f *= 2.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;
    a *= 0.5f;
    f *= 2.0f;
    perlin_value += a * perlin(xyz, s * f, tile).r;

    return perlin_value;
}

vec3 voronoi_hash(vec3 x, float s)
{
    x = mod(x, s);
    x = vec3(dot(x, vec3(127.1f, 311.7f, 74.7f)),
    dot(x, vec3(269.5f, 183.3f, 246.1f)),
    dot(x, vec3(113.5f, 271.9f, 124.6f)));

    return fract(sin(x) * 43758.5453123f);
}

vec3 voronoi(in vec3 x, float s, float seed, bool inverted)
{
    x *= s;
    x += 0.5;
    vec3 p = floor(x);
    vec3 f = fract(x);

    float id = 0.0;
    vec2 res = vec2(1.0, 1.0);
    for (int k = -1; k <= 1; k++)
    {
        for (int j = -1; j <= 1; j++)
        {
            for (int i = -1; i <= 1; i++)
            {
                vec3 b = vec3(i, j, k);
                vec3 r = vec3(b) - f + voronoi_hash(p + b + seed * 10.0f, s);
                float d = dot(r, r);

                if (d < res.x)
                {
                    id = dot(p + b, vec3(1.0f, 57.0f, 113.0f));
                    res = vec2(d, res.x);
                }
                else if (d < res.y)
                {
                    res.y = d;
                }
            }
        }
    }

    vec2 result = res;
    id = abs(id);

    if (inverted)
        return vec3(1.0f - result, id);
    else
        return vec3(result, id);
}

float worley(vec3 p, float s, float seed) {
    return voronoi(p, s, seed, true).r;
}

float worley(vec3 p, float s) {
    return voronoi(p, s, 0.0f, true).r;
}

float worley_2_octaves(vec3 p, float s, float seed)
{
    vec3 xyz = p;

    float worley_value1 = voronoi(xyz, 1.0f * s, seed, true).r;
    float worley_value2 = voronoi(xyz, 2.0f * s, seed, false).r;

    worley_value1 = clamp(worley_value1, 0.0f, 1.0f);
    worley_value2 = clamp(worley_value2, 0.0f, 1.0f);

    return worley_value1 * 0.625f - worley_value2 * 0.25f;
}

float worley_2_octaves(vec3 p, float s)
{
    return worley_2_octaves(p, s, 0.0f);
}

float worley_3_octaves(vec3 p, float s, float seed)
{
    vec3 xyz = p;

    float worley_value1 = voronoi(xyz, 2.0f * s, seed, true).r;
    float worley_value2 = voronoi(xyz, 8.0f * s, seed, false).r;
    float worley_value3 = voronoi(xyz, 14.0f * s, seed, false).r;

    worley_value1 = clamp(worley_value1, 0.0f, 1.0f);
    worley_value2 = clamp(worley_value2, 0.0f, 1.0f);
    worley_value3 = clamp(worley_value3, 0.0f, 1.0f);

    return worley_value1 * 0.625f - worley_value2 * 0.25f - worley_value3 * 0.125f;
}

float worley_3_octaves(vec3 p, float s)
{
    return worley_3_octaves(p, s, 0.0f);
}

float alligator(vec3 p, float s, float seed, bool inverted) {
    const vec3 voronoi = voronoi(p, s, seed, true);
    if (!inverted)
        return 1 - voronoi.y - voronoi.x;
    
    return voronoi.x - voronoi.y;
}

float alligator(vec3 p, float s, float seed) {
    const vec3 voronoi = voronoi(p, s, seed, true);
    
    return voronoi.x - voronoi.y;
}

float alligator(vec3 p, float s) {
    return alligator(p, s, 0, true);
}

float alligator_2_octaves(vec3 p, float s, float seed)
{
    vec3 xyz = p;

    float alligator_value1 = alligator(xyz, 1.0f * s, seed, true);
    float alligator_value2 = alligator(xyz, 2.0f * s, seed, true);

    return alligator_value1 + alligator_value2 * 0.5f;
}

float alligator_2_octaves(vec3 p, float s)
{
    return alligator_2_octaves(p, s, 0.0f);
}

float alligator_3_octaves(vec3 p, float s, float seed)
{
    vec3 xyz = p;

    float alligator_value1 = alligator(xyz, 1.0f * s, seed, true);
    float alligator_value2 = alligator(xyz, 2.0f * s, seed, true);
    float alligator_value3 = alligator(xyz, 4.0f * s, seed, true);

    return alligator_value1 + alligator_value2 * 0.5f + alligator_value3 * 0.25f;
}

float alligator_3_octaves(vec3 p, float s)
{
    return alligator_3_octaves(p, s, 0.0f);
}

float perlin_worley(vec3 p, float p_freq, float w_freq, vec2 p_min_max, vec2 w_min_max, float mix_multiplier) {
    const float perlin = remap_01(perlin_7_octaves(p, p_freq, true), p_min_max.x, p_min_max.y);
    const float worley = remap_01(worley_3_octaves(p, w_freq), w_min_max.x, w_min_max.y);

    const float perlin_worley = remap_clamp(worley, 0.0f, 1.0f, perlin * mix_multiplier, 1.0f);
    
    return perlin_worley;
}

vec3 curl_noise(vec3 uv, float frequency) {
    const float epsilon = 0.05f;
    float noise1, noise2, a, b;
    vec3 c;

    const vec3 pos = uv * frequency;
    noise1 = perlin_5_octaves(pos.xyz + vec3(0.0f, epsilon, 0.0), false);
    noise2 = perlin_5_octaves(pos.xyz + vec3(0.0f, -epsilon, 0.0), false);
    a = (noise1 - noise2) / (2 * epsilon);
    noise1 = perlin_5_octaves(pos.xyz + vec3(0.0f, 0.0f, epsilon), false);
    noise2 = perlin_5_octaves(pos.xyz + vec3(0.0f, 0.0f, -epsilon), false);
    b = (noise1 - noise2) / (2 * epsilon);

    c.x = a - b;

    noise1 = perlin_5_octaves(pos.xyz + vec3(0.0f, 0.0f,  epsilon), false);
    noise2 = perlin_5_octaves(pos.xyz + vec3(0.0f, 0.0f, -epsilon), false);
    a = (noise1 - noise2) / (2 * epsilon);
    noise1 = perlin_5_octaves(pos.xyz + vec3( epsilon, 0.0f, 0.0), false);
    noise2 = perlin_5_octaves(pos.xyz + vec3(-epsilon, 0.0f, 0.0), false);
    b = (noise1 - noise2) / (2 * epsilon);

    c.y = a - b;

    noise1 = perlin_5_octaves(pos.xyz + vec3( epsilon, 0.0f, 0.0), false);
    noise2 = perlin_5_octaves(pos.xyz + vec3(-epsilon, 0.0f, 0.0), false);
    a = (noise1 - noise2) / (2 * epsilon);
    noise1 = perlin_5_octaves(pos.xyz + vec3(0.0f,  epsilon, 0.0), false);
    noise2 = perlin_5_octaves(pos.xyz + vec3(0.0f, -epsilon, 0.0), false);
    b = (noise1 - noise2) / (2 * epsilon);

    c.z = a - b;

    const float remap_low = -0.5;
    const float remap_high = 3.0;
    vec3 noise = remap_01_noclamp(c, remap_low, remap_high);
    
    return noise;
}

float curly_alligator(vec3 p, float freq) {
    const float alligator = 1.0 - alligator_3_octaves(p + curl_noise(p, freq) / 20, freq);
    
    return alligator;
}