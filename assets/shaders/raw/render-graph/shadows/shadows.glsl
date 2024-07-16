#include "poisson_samples.glsl"

float linearize_depth(float z, uint cascade_index) {
    const float f = u_csm_data.csm.far[cascade_index];
    const float n = u_csm_data.csm.near[cascade_index];
    
    return (f - n) * z - f;
}

vec2 find_occluder(float receiver_z, vec3 uvz, float light_size_uv, uint cascade_index) {
    const float search_width = light_size_uv * (receiver_z + u_csm_data.csm.near[cascade_index]) / receiver_z;
    
    float sum = 0.0f;
    float count = 0.0f;
    
    const uint poisson_samples = 16;
    for (uint i = 0; i < poisson_samples; i++) {
        const vec2 uv = search_width * POISSON_16[i] + uvz.xy;
        const float depth = textureLod(sampler2DArray(u_csm, u_sampler_shadow), vec3(uv, float(cascade_index)), 0).r;
        if (depth > uvz.z) {
            count++;
            sum += depth;
        }
    }
    
    return vec2(linearize_depth(sum / count, cascade_index), count);
}

float sample_shadow(vec3 normal, vec3 uvz, vec2 delta, float cascade) {
    const float depth = textureLod(sampler2DArray(u_csm, u_sampler_shadow), vec3(uvz.xy + delta, cascade), 0).r;
    const float shadow = depth < uvz.z ? 0.0f : 0.9f;

    return shadow;
}

float pcf_sample_shadow_poisson(vec3 uvz, vec3 normal, float scale, uint cascade_index) {
    float shadow_factor = 0.0f;
    
    const uint poisson_samples = 16;
    for (uint i = 0; i < poisson_samples; i++) {
        shadow_factor += sample_shadow(normal, uvz, scale * POISSON_16[i], float(cascade_index));
    }
    shadow_factor /= float(poisson_samples);

    return shadow_factor;
}


float pcf_sample_shadow(vec3 uvz, vec3 normal, vec2 delta, uint cascade_index) {
    float shadow_factor = 0.0f;
    int samples_dim = 1;
    int samples_count = 0;
    for (int x = -samples_dim; x <= samples_dim; x++) {
        for (int y = -samples_dim; y <= samples_dim; y++) {
            shadow_factor += sample_shadow(normal, uvz, vec2(delta.x * x, delta.y * y), float(cascade_index));
            samples_count++;
        }
    }
    shadow_factor /= float(samples_count);

    return shadow_factor;
}

float pcss_sample_shadow(vec3 position, vec3 uvz, vec3 normal, float light_size_uv, vec2 delta, uint cascade_index) {
    float receiver_z = (u_csm_data.csm.views[cascade_index] * vec4(position, 1.0f)).z;
    
    const vec2 occluder_info = find_occluder(receiver_z, uvz, light_size_uv, cascade_index);
    if (occluder_info.y == 0)
        return 0.0f;
    
    float penumbra_width = (receiver_z - occluder_info.x) * light_size_uv / occluder_info.x;
    float filter_radius = penumbra_width;
    
    return pcf_sample_shadow_poisson(uvz, normal, filter_radius, cascade_index);
}

float sample_shadow_cascade(vec3 position, vec3 normal, float light_size_uv, vec2 delta, uint cascade_index) {
    const vec4 shadow_local = u_csm_data.csm.view_projections[cascade_index] * vec4(position, 1.0f);
    const vec3 ndc = shadow_local.xyz / shadow_local.w;
    vec3 uvz = vec3(vec2(ndc.xy * 0.5f) + 0.5f, ndc.z);

    if (uvz.z < 0.0f || uvz.z > 1.0f)
        return 0.0f;

    const float SHADOW_BIAS_MAX = 0.0025f;
    const float SHADOW_BIAS_MIN = 0.0005f;

    const float bias = SHADOW_BIAS_MAX;
    const float n_dot_l = dot(normal, -u_directional_light.light.direction);
    const float normal_bias = mix(bias, SHADOW_BIAS_MIN, n_dot_l);
    uvz.z += normal_bias;
    
    if (u_shading.settings.soft_shadows)
        return pcss_sample_shadow(position, uvz, normal, light_size_uv, delta, cascade_index);
    
    return pcf_sample_shadow_poisson(uvz, normal, delta.x * 2.0f, cascade_index);
}

float shadow(vec3 position, vec3 normal, float light_size) {
    const float SEEMS_THRESHOLD = 0.9f;
    
    const ivec2 shadow_size = ivec2(textureSize(u_csm, 0));
    const float light_size_uv = light_size / max(shadow_size.x, shadow_size.y);
    const vec2 delta = vec2(1.0f) / vec2(shadow_size);

    const vec3 position_view = vec3(u_camera.camera.view * vec4(position, 1.0f));
    uint cascade_index = 0;
    for (uint i = 0; i < u_csm_data.csm.cascade_count; i++)
        if (-position_view.z < u_csm_data.csm.cascades[i]) {
            cascade_index = i;
            break;
        }
    
    const float shadow = sample_shadow_cascade(position, normal, light_size_uv, delta, cascade_index);
    // blend betwee cascades, if too close to the end of current cascade
    const float cascade_relative_distance = cascade_index < u_csm_data.csm.cascade_count ?
        -position_view.z / u_csm_data.csm.cascades[cascade_index] :
        0.0f;
    if (cascade_relative_distance > SEEMS_THRESHOLD) {
        const float shadow_next = sample_shadow_cascade(position, normal, light_size_uv, delta, cascade_index + 1);
        
        return mix(shadow, shadow_next,
            (cascade_relative_distance - SEEMS_THRESHOLD) / (1.0f - SEEMS_THRESHOLD));
    }

    return shadow;

}