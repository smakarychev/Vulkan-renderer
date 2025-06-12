// based on "A Scalable and Production Ready Sky and Atmosphere Rendering Technique by Sébastien Hillaire (Epic Games, Inc)"
// https://github.com/sebh/UnrealEngineSkyAtmosphere

struct Scattering {
    vec3 L;
    vec3 OpticalDepth;
    vec3 Transmittance;
    vec3 Multiscattering;
};

Scattering integrate_scattered_luminance(vec2 uv, vec3 ro, vec3 rd, vec3 sun_dir, ViewInfo view,
    float sample_count, bool surface, vec3 global_l, bool use_uniform_phase, float max_depth) {
    Scattering scattering;

    const vec3 center = vec3(0.0f);
    const Intersection atmosphere_intersection = intersect_sphere(ro, rd, center, view.atmosphere);
    const Intersection surface_intersection = intersect_sphere(ro, rd, center, view.surface);

    float depth = atmosphere_intersection.depth;
    const float to_surface_depth = surface_intersection.t - atmosphere_intersection.t;
    if (surface_intersection.t == NO_HIT) {
        if (atmosphere_intersection.t == NO_HIT)
            return scattering;
    } else {
        depth = min(depth, to_surface_depth);
    }
    depth = min(depth, max_depth);

    vec3 luminance = vec3(0.0f);
    vec3 optical_depth = vec3(0.0f);
    vec3 throughput = vec3(1.0f);

    const float cos_theta = dot(rd, sun_dir);
    const float rayleigh = rayleigh_phase(cos_theta);
    const float mie = mie_phase(cos_theta);
    const float uniform_phase = 1.0f / (4.0f * PI);
    const float sample_t = 0.3f;

    float t = 0.0f;

    for (float i = 0.0f; i < sample_count; i += 1.0f) {
        const float new_t = depth * (i + sample_t) / sample_count;
        const float dt = new_t - t;
        t = new_t;
        const vec3 x = ro + rd * t;
        const MediaSample media = sample_media(x, vec3(0.0f), view);

        const vec3 sample_optical_depth = media.extinction * dt;
        const vec3 sample_transmittance = exp(-sample_optical_depth);
        optical_depth += sample_optical_depth;

        const float r = length(x);
        const vec3 up = x / r;
        const float mu = dot(sun_dir, up);
        const vec2 transmittance_uv = transmittance_uv_from_r_mu(view, r, mu);
        const vec3 transmittance = textureLod(sampler2D(u_transmittance_lut, u_sampler), transmittance_uv, 0).rgb;

        vec3 phase_scattering;
        if (use_uniform_phase)
            phase_scattering = (media.rayleigh + media.mie) * uniform_phase;
        else
            phase_scattering = media.rayleigh * rayleigh + media.mie * mie;

        const float surface_shadow = get_visibility(view, x, sun_dir, vec3(0.0f) + PLANET_RADIUS_OFFSET_KM * up);
        
        float shadow_map = 0.0f;
        #ifdef WITH_SHADOW_MAP
            const vec3 shadow_ws = view.position + rd * t * vec3(-1, 1, -1);
            const float shadow_z_view = (view.view * vec4(shadow_ws, 1.0f)).z;
            shadow_map = shadow(view.position + rd * t * vec3(-1, 1, -1), vec3(0, 0, 0),
                u_directional_lights.lights[0].direction, u_directional_lights.lights[0].size, shadow_z_view);
        #endif // WITH_SHADOW_MAP
        
        vec3 multiscattering_luminance = vec3(0.0f);
        #ifdef WITH_MULTISCATTERING
            const vec2 multiscattering_uv = multiscattering_uv_from_r_mu(view, r, mu);
            multiscattering_luminance = textureLod(sampler2D(u_multiscattering_lut, u_sampler), multiscattering_uv, 0).rgb;
        #endif // WITH_MULTISCATTERING

        const vec3 MS = (media.rayleigh + media.mie) * 1;
        const vec3 MS_integral = (MS - MS * sample_transmittance) / media.extinction;
        scattering.Multiscattering += throughput * MS_integral;
        
        const vec3 S = global_l * (
            (1.0f - shadow_map) * surface_shadow * phase_scattering * transmittance +
            multiscattering_luminance * MS);
        const vec3 S_integral = (S - S * sample_transmittance) / media.extinction;
        luminance += throughput * S_integral;

        throughput *= sample_transmittance;
    }

    if (surface && depth == to_surface_depth) {
        const vec3 x = ro + rd * depth;
        const float r = length(x);
        const vec3 up = x / r;
        const float mu = dot(up, sun_dir);
        const vec2 transmittance_uv = transmittance_uv_from_r_mu(view, r, mu);
        const vec3 transmittance = textureLod(sampler2D(u_transmittance_lut, u_sampler), transmittance_uv, 0).rgb;

        const float n_dot_l = clamp(dot(up, sun_dir), 0.0f, 1.0f);
        luminance += global_l * n_dot_l * transmittance * throughput * view.surface_albedo.rgb / PI;
    }

    scattering.L = luminance;
    scattering.OpticalDepth = optical_depth;
    scattering.Transmittance = throughput;

    return scattering;
}