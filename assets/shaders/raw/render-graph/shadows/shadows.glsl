#include "poisson_samples.glsl"

#extension GL_EXT_samplerless_texture_functions: require

float linearize_depth(float z, uint cascade_index) {
    const float f = u_csm_data.csm.far[cascade_index];
    const float n = u_csm_data.csm.near[cascade_index];
    
    return (f - n) * z - f;
}

float random(vec4 seed) {
    float dot_product = dot(seed, vec4(12.9898,78.233,45.164,94.673));
    return fract(sin(dot_product) * 43758.5453);
}

mat2 random_rotation(vec4 seed) {
    const float theta = random(seed);
    
    return mat2(cos(theta), sin(theta), -sin(theta), cos(theta));
}

vec2 find_occluder(float receiver_z, vec3 uvz, float light_size_uv, uint cascade_index) {
    const float search_width = light_size_uv * (receiver_z + u_csm_data.csm.near[cascade_index]) / receiver_z;
    
    float sum = 0.0f;
    float count = 0.0f;
    
    const mat2 random_rotation = random_rotation(uvz.xyzz);
    
    const uint poisson_samples = 16;
    for (uint i = 0; i < poisson_samples; i++) {
        const vec2 uv = random_rotation * search_width * POISSON_16[i] + uvz.xy;
        const float depth = textureLod(sampler2DArray(u_csm, u_sampler_shadow), vec3(uv, float(cascade_index)), 0).r;
        if (depth > uvz.z) {
            count++;
            sum += depth;
        }
    }
    
    return vec2(-linearize_depth(sum / count, cascade_index), count);
}

float sample_shadow(vec3 uvz, vec2 delta, float cascade) {
    const float shadow = texture(
        sampler2DArrayShadow(u_csm, u_sampler_shadow), vec4(uvz.xy + delta, cascade, uvz.z)).r;

    return shadow;
}

float sample_shadow_for_pcf(vec2 base_uv, float u, float v, vec2 shadow_size_inv, float depth, uint cascade_index) {
    return sample_shadow(vec3(base_uv, depth), vec2(u, v) * shadow_size_inv, cascade_index);
}

#ifndef FILTER_SIZE
#define FILTER_SIZE 7
#endif // FILTER_SIZE

float pcf_optimized_shadow(vec3 uvz, uint cascade_index) {
    // implementation is almost identical to https://github.com/TheRealMJP/Shadows
    
    const vec2 shadow_size = vec2(textureSize(u_csm, 0));
    const vec2 shadow_size_inv = 1.0f / shadow_size;
    
    const vec2 uv = uvz.xy * shadow_size;
    vec2 base_uv = vec2(floor(uv.x + 0.5f), floor(uv.y + 0.5f));
    const float s = uv.x + 0.5f - base_uv.x;
    const float t = uv.y + 0.5f - base_uv.y;

    base_uv -= vec2(0.5f, 0.5f);
    base_uv *= shadow_size_inv;

    float sum = 0;

    #if FILTER_SIZE == 2
        return sample_shadow(uvz, vec2(0), cascade_index);
    #elif FILTER_SIZE == 3
        const float uw0 = (3 - 2 * s);
        const float uw1 = (1 + 2 * s);
    
        const float u0 = (2 - s) / uw0 - 1;
        const float u1 = s / uw1 + 1;
    
        const float vw0 = (3 - 2 * t);
        const float vw1 = (1 + 2 * t);
    
        const float v0 = (2 - t) / vw0 - 1;
        const float v1 = t / vw1 + 1;
    
        sum += uw0 * vw0 * sample_shadow_for_pcf(base_uv, u0, v0, shadow_size_inv, uvz.z, cascade_index);
        sum += uw1 * vw0 * sample_shadow_for_pcf(base_uv, u1, v0, shadow_size_inv, uvz.z, cascade_index);
        sum += uw0 * vw1 * sample_shadow_for_pcf(base_uv, u0, v1, shadow_size_inv, uvz.z, cascade_index);
        sum += uw1 * vw1 * sample_shadow_for_pcf(base_uv, u1, v1, shadow_size_inv, uvz.z, cascade_index);

        return sum * 1.0f / 16;
    #elif FILTER_SIZE == 5
        const float uw0 = (4 - 3 * s);
        const float uw1 = 7;
        const float uw2 = (1 + 3 * s);
    
        const float u0 = (3 - 2 * s) / uw0 - 2;
        const float u1 = (3 + s) / uw1;
        const float u2 = s / uw2 + 2;
    
        const float vw0 = (4 - 3 * t);
        const float vw1 = 7;
        const float vw2 = (1 + 3 * t);
    
        const float v0 = (3 - 2 * t) / vw0 - 2;
        const float v1 = (3 + t) / vw1;
        const float v2 = t / vw2 + 2;
    
        sum += uw0 * vw0 * sample_shadow_for_pcf(base_uv, u0, v0, shadow_size_inv, uvz.z, cascade_index);
        sum += uw1 * vw0 * sample_shadow_for_pcf(base_uv, u1, v0, shadow_size_inv, uvz.z, cascade_index);
        sum += uw2 * vw0 * sample_shadow_for_pcf(base_uv, u2, v0, shadow_size_inv, uvz.z, cascade_index);
    
        sum += uw0 * vw1 * sample_shadow_for_pcf(base_uv, u0, v1, shadow_size_inv, uvz.z, cascade_index);
        sum += uw1 * vw1 * sample_shadow_for_pcf(base_uv, u1, v1, shadow_size_inv, uvz.z, cascade_index);
        sum += uw2 * vw1 * sample_shadow_for_pcf(base_uv, u2, v1, shadow_size_inv, uvz.z, cascade_index);
    
        sum += uw0 * vw2 * sample_shadow_for_pcf(base_uv, u0, v2, shadow_size_inv, uvz.z, cascade_index);
        sum += uw1 * vw2 * sample_shadow_for_pcf(base_uv, u1, v2, shadow_size_inv, uvz.z, cascade_index);
        sum += uw2 * vw2 * sample_shadow_for_pcf(base_uv, u2, v2, shadow_size_inv, uvz.z, cascade_index);
    
        return sum * 1.0f / 144;
    #else // FILTER_SIZE == 7
        const float uw0 = (5 * s - 6);
        const float uw1 = (11 * s - 28);
        const float uw2 = -(11 * s + 17);
        const float uw3 = -(5 * s + 1);
    
        const float u0 = (4 * s - 5) / uw0 - 3;
        const float u1 = (4 * s - 16) / uw1 - 1;
        const float u2 = -(7 * s + 5) / uw2 + 1;
        const float u3 = -s / uw3 + 3;
    
        const float vw0 = (5 * t - 6);
        const float vw1 = (11 * t - 28);
        const float vw2 = -(11 * t + 17);
        const float vw3 = -(5 * t + 1);
    
        const float v0 = (4 * t - 5) / vw0 - 3;
        const float v1 = (4 * t - 16) / vw1 - 1;
        const float v2 = -(7 * t + 5) / vw2 + 1;
        const float v3 = -t / vw3 + 3;
    
        sum += uw0 * vw0 * sample_shadow_for_pcf(base_uv, u0, v0, shadow_size_inv, uvz.z, cascade_index);
        sum += uw1 * vw0 * sample_shadow_for_pcf(base_uv, u1, v0, shadow_size_inv, uvz.z, cascade_index);
        sum += uw2 * vw0 * sample_shadow_for_pcf(base_uv, u2, v0, shadow_size_inv, uvz.z, cascade_index);
        sum += uw3 * vw0 * sample_shadow_for_pcf(base_uv, u3, v0, shadow_size_inv, uvz.z, cascade_index);
    
        sum += uw0 * vw1 * sample_shadow_for_pcf(base_uv, u0, v1, shadow_size_inv, uvz.z, cascade_index);
        sum += uw1 * vw1 * sample_shadow_for_pcf(base_uv, u1, v1, shadow_size_inv, uvz.z, cascade_index);
        sum += uw2 * vw1 * sample_shadow_for_pcf(base_uv, u2, v1, shadow_size_inv, uvz.z, cascade_index);
        sum += uw3 * vw1 * sample_shadow_for_pcf(base_uv, u3, v1, shadow_size_inv, uvz.z, cascade_index);
    
        sum += uw0 * vw2 * sample_shadow_for_pcf(base_uv, u0, v2, shadow_size_inv, uvz.z, cascade_index);
        sum += uw1 * vw2 * sample_shadow_for_pcf(base_uv, u1, v2, shadow_size_inv, uvz.z, cascade_index);
        sum += uw2 * vw2 * sample_shadow_for_pcf(base_uv, u2, v2, shadow_size_inv, uvz.z, cascade_index);
        sum += uw3 * vw2 * sample_shadow_for_pcf(base_uv, u3, v2, shadow_size_inv, uvz.z, cascade_index);
    
        sum += uw0 * vw3 * sample_shadow_for_pcf(base_uv, u0, v3, shadow_size_inv, uvz.z, cascade_index);
        sum += uw1 * vw3 * sample_shadow_for_pcf(base_uv, u1, v3, shadow_size_inv, uvz.z, cascade_index);
        sum += uw2 * vw3 * sample_shadow_for_pcf(base_uv, u2, v3, shadow_size_inv, uvz.z, cascade_index);
        sum += uw3 * vw3 * sample_shadow_for_pcf(base_uv, u3, v3, shadow_size_inv, uvz.z, cascade_index);
    
        return sum * 1.0f / 2704;
    #endif
}

float pcf_sample_shadow_poisson(vec3 uvz, vec3 normal, float scale, uint cascade_index) {
    float shadow_factor = 0.0f;

    const mat2 random_rotation = random_rotation(uvz.xyzz);
    
    const uint poisson_samples = 16;
    for (uint i = 0; i < poisson_samples; i++) {
        shadow_factor += sample_shadow(uvz, random_rotation * scale * POISSON_16[i], float(cascade_index));
    }
    shadow_factor /= float(poisson_samples);

    return shadow_factor;
}

float pcss_sample_shadow(vec3 position, vec3 uvz, vec3 normal, float light_size_uv, uint cascade_index) {
    float receiver_z = (u_csm_data.csm.views[cascade_index] * vec4(position, 1.0f)).z;
    
    const vec2 occluder_info = find_occluder(receiver_z, uvz, light_size_uv, cascade_index);
    if (occluder_info.y == 0)
        return 0.0f;
    
    float penumbra_width = (receiver_z - occluder_info.x) * light_size_uv / occluder_info.x;
    float filter_radius = u_csm_data.csm.near[cascade_index] * penumbra_width / receiver_z;

    return pcf_sample_shadow_poisson(uvz, normal, filter_radius, cascade_index);
}

vec3 get_shadow_offset(vec3 normal, vec3 light_direction) {
    const vec2 shadow_size = vec2(textureSize(u_csm, 0));
    const float texel_size = 2.0f / shadow_size.x;
    const float normal_offset_scale = clamp(1.0f - dot(normal, light_direction), 0.0f, 1.0f);
    
    return texel_size * normal_offset_scale * normal;
}

float sample_shadow_cascade(vec3 ndc, vec3 normal, float light_size_uv, vec2 delta, uint cascade_index) {
    const float const_bias = 5e-3f;
    vec3 uvz = vec3(vec2(ndc.xy * 0.5f) + 0.5f, ndc.z);
    uvz.z += const_bias;
    return pcf_optimized_shadow(uvz, cascade_index);
}

float shadow(vec3 position, vec3 normal, float light_size, float z_view) {
    const float SEEMS_THRESHOLD = 0.9f;

    z_view = -z_view;
    
    const ivec2 shadow_size = ivec2(textureSize(u_csm, 0));
    const float light_size_uv = light_size / max(shadow_size.x, shadow_size.y);
    const vec2 delta = vec2(1.0f) / vec2(shadow_size);

    uint cascade_index = u_csm_data.csm.cascade_count - 1; 
    
    for (uint i = 0; i < u_csm_data.csm.cascade_count; i++) {
        if (z_view < u_csm_data.csm.cascades[i]) {
            cascade_index = i;
            break;
        }
    }
    const vec3 position_offset = 
        get_shadow_offset(normal, u_directional_light.light.direction) / u_csm_data.csm.cascades[cascade_index];
    vec4 position_local = u_csm_data.csm.view_projections[cascade_index] * vec4(position + position_offset, 1.0f);
    const float shadow = 
        sample_shadow_cascade(position_local.xyz / position_local.w, normal, light_size_uv, delta, cascade_index);
    // blend between cascades, if too close to the end of current cascade
    const uint next_cascade = cascade_index + 1;
    if (next_cascade >= u_csm_data.csm.cascade_count) {
        return shadow;
    }

    const float cascade_split = u_csm_data.csm.cascades[cascade_index];
    const float cascade_size = cascade_split - (cascade_index == 0 ?
        0.0f : u_csm_data.csm.cascades[cascade_index - 1]);
    const float cascade_relative_distance = 1.0 - (cascade_split - z_view) / cascade_size;

    if (cascade_relative_distance > SEEMS_THRESHOLD) {
        const vec3 position_offset_next =
            get_shadow_offset(normal, u_directional_light.light.direction) / u_csm_data.csm.cascades[next_cascade];
        position_local = u_csm_data.csm.view_projections[next_cascade] * vec4(position + position_offset_next, 1.0f);
        if (!all(bvec3(abs(position_local.x) < 0.99f, abs(position_local.y) < 0.99f, abs(position_local.z - 0.499f) < 0.499f)))
            return shadow;
        const float shadow_next = 
            sample_shadow_cascade(position_local.xyz / position_local.w, normal, light_size_uv, delta, next_cascade);
        return mix(shadow, shadow_next,
            (cascade_relative_distance - SEEMS_THRESHOLD) / (1.0f - SEEMS_THRESHOLD));
    }

    return shadow;
}