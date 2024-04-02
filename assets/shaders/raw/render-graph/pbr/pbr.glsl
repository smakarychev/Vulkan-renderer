const float PI = 3.14159265359f;
const float HALF_PI = 3.14159265359f * 0.5f;
const float TAU = PI * 2.0f;
const float PI_INV = 1.0f /  PI;

float d_ggx(float n_dot_h, float roughness) {
    const float a2 = roughness * roughness;
    const float f = (n_dot_h * a2 - n_dot_h) * n_dot_h + 1.0;
    
    return a2 / (PI * f * f);
}

float v_smith_correlated(float n_dot_v, float n_dot_l, float roughness) {
    const float a2 = roughness * roughness;
    const float v = 0.5f / mix(2 * n_dot_l * n_dot_v, n_dot_l + n_dot_v, a2);
    
    return clamp(v, 0.0f, 3.402823466e+38f); 
}

vec3 fresnel_schlick(float cos_theta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

vec3 fresnel_schlick_roughness(float cos_theta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}