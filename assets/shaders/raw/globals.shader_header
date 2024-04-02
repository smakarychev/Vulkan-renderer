const float PI = 3.14159265359f;
const float HALF_PI = 3.14159265359f * 0.5f;
const float TAU = PI * 2.0f;
const float PI_INV = 1.0f /  PI;

vec3 cubemap_normal_vector(uvec3 global_invocation, vec2 size_inv) {
    const vec2 st = vec2(global_invocation.xy + 0.5f) * size_inv;
    const vec2 uv = 2.0f * vec2(st.x, 1.0f - st.y) - vec2(1.0f);

    vec3 ret;
    if (global_invocation.z == 0)
        ret = vec3(1.0,  uv.y, -uv.x);
    else if (global_invocation.z == 1)
        ret = vec3(-1.0, uv.y,  uv.x);
    else if (global_invocation.z == 2)
        ret = vec3(uv.x, 1.0, -uv.y);
    else if (global_invocation.z == 3)
        ret = vec3(uv.x, -1.0, uv.y);
    else if (global_invocation.z == 4)
        ret = vec3(uv.x, uv.y, 1.0);
    else if (global_invocation.z == 5)
        ret = vec3(-uv.x, uv.y, -1.0);
    
    return normalize(ret);
}

vec2 hammersley2d(uint i, uint n) {
    // http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
    
    uint radical_inverse_i = i;
    radical_inverse_i = (radical_inverse_i << 16u) | (radical_inverse_i >> 16u);
    radical_inverse_i = ((radical_inverse_i & 0x55555555u) << 1u) | ((radical_inverse_i & 0xAAAAAAAAu) >> 1u);
    radical_inverse_i = ((radical_inverse_i & 0x33333333u) << 2u) | ((radical_inverse_i & 0xCCCCCCCCu) >> 2u);
    radical_inverse_i = ((radical_inverse_i & 0x0F0F0F0Fu) << 4u) | ((radical_inverse_i & 0xF0F0F0F0u) >> 4u);
    radical_inverse_i = ((radical_inverse_i & 0x00FF00FFu) << 8u) | ((radical_inverse_i & 0xFF00FF00u) >> 8u);

    const float radical_inverse = float(radical_inverse_i) * 2.3283064365386963e-10;
    
    return vec2(float(i) / float(n), radical_inverse);
}

vec3 sample_hemisphere(vec2 uv, vec3 normal) {
    const float u = uv.x;
    const float v = uv.y;

    const float phi = u * TAU;
    const float cos_theta = 1.0f - v;
    const float sin_theta = sqrt(1.0f - cos_theta * cos_theta);
    
    const vec3 local = vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
    
    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    const vec3 tangent = normalize(cross(up, normal));
    const vec3 bitangent = normalize(cross(normal, tangent));
    
    return normalize(local.x * tangent + local.y * bitangent + local.z * normal);
}

vec3 importance_sample_ggx(vec2 uv, vec3 normal, float roughness) {
    const float a = roughness * roughness;
        
    const float u = uv.x;
    const float v = uv.y;
    
    const float phi = u * TAU;
    float cos_theta = sqrt((1.0 - v) / (1.0 + (a * a - 1.0) * v));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    
    const vec3 local = vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
    
    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    const vec3 tangent = normalize(cross(up, normal));
    const vec3 bitangent = normalize(cross(normal, tangent));
    
    return normalize(local.x * tangent + local.y * bitangent + local.z * normal);
}

float d_ggx(float n_dot_h, float roughness) {
    float a2 = roughness * roughness;
    
    float f = (n_dot_h * a2 - n_dot_h) * n_dot_h + 1.0;
    
    return a2 / (PI * f * f);
}

float geometry_schlick_ggx(float n_dot_v, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0f;

    float nom = n_dot_v;
    float denom = n_dot_v * (1.0f - k) + k;

    return nom / denom;
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float n_dot_v = max(dot(N, V), 0.0);
    float n_dot_l = max(dot(N, L), 0.0);
    float ggx2 = geometry_schlick_ggx(n_dot_v, roughness);
    float ggx1 = geometry_schlick_ggx(n_dot_l, roughness);

    return ggx1 * ggx2;
}  