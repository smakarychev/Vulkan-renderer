float sample_shadow(vec3 normal, float projected_depth, vec2 uv, vec2 delta, float cascade) {
    const float SHADOW_BIAS_MAX = 0.0025f;
    const float SHADOW_BIAS_MIN = 0.0005f;
    
    const float bias = SHADOW_BIAS_MAX;
    const float n_dot_l = dot(normal, -u_directional_light.light.direction);
    const float normal_bias = mix(bias, SHADOW_BIAS_MIN, n_dot_l);

    const float depth = textureLod(sampler2DArray(u_csm, u_sampler_shadow), vec3(uv + delta, cascade), 0).r;
    const float shadow = depth < projected_depth + normal_bias ? 0.0f : 0.9f;

    return shadow;
}

float pcf_sample_shadow(vec3 position, vec3 normal, vec2 delta, uint cascade_index) {
    const vec4 shadow_local = u_csm_data.csm.view_projections[cascade_index] * vec4(position, 1.0f);
    const vec3 ndc = shadow_local.xyz / shadow_local.w;
    vec2 uv = (ndc.xy * 0.5f) + 0.5f;

    if (ndc.z < 0.0f)
    return 0.0f;

    float shadow_factor = 0.0f;
    int samples_dim = 1;
    int samples_count = 0;
    for (int x = -samples_dim; x <= samples_dim; x++) {
        for (int y = -samples_dim; y <= samples_dim; y++) {
            shadow_factor += sample_shadow(normal, ndc.z, uv, vec2(delta.x * x, delta.y * y), float(cascade_index));
            samples_count++;
        }
    }
    shadow_factor /= float(samples_count);

    return shadow_factor;
}

float shadow(vec3 position, vec3 normal) {
    
    const float SEEMS_THRESHOLD = 0.9f;
    
    const ivec2 shadow_size = ivec2(textureSize(u_csm, 0));
    const float scale = 0.8f;
    const vec2 delta = vec2(scale) / vec2(shadow_size);

    const vec3 position_view = vec3(u_camera.camera.view * vec4(position, 1.0f));
    uint cascade_index = 0;
    for (uint i = 0; i < u_csm_data.csm.cascade_count; i++)
        if (-position_view.z < u_csm_data.csm.cascades[i]) {
            cascade_index = i;
            break;
        }
    
    const float shadow = pcf_sample_shadow(position, normal, delta, cascade_index);
    // blend betwee cascades, if too close to the end of current cascade
    const float cascade_relative_distance = cascade_index < u_csm_data.csm.cascade_count ?
        -position_view.z / u_csm_data.csm.cascades[cascade_index] :
        0.0f;
    if (cascade_relative_distance > SEEMS_THRESHOLD) {
        const float shadow_next = pcf_sample_shadow(position, normal, delta, cascade_index + 1);
        
        return mix(shadow, shadow_next,
            (cascade_relative_distance - SEEMS_THRESHOLD) / (1.0f - SEEMS_THRESHOLD));
    }

    return shadow;

}