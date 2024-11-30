// based on "A Scalable and Production Ready Sky and Atmosphere Rendering Technique by Sébastien Hillaire (Epic Games, Inc)"
// https://github.com/sebh/UnrealEngineSkyAtmosphere

#version 460

#include "common.glsl"
#include "../tonemapping.glsl"
#include "../../utility.glsl"

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vertex_uv;

@immutable_sampler_clamp_edge
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_depth;
layout(set = 1, binding = 1) uniform texture2D u_sky_view_lut;
layout(set = 1, binding = 2) uniform texture2D u_transmittance_lut;
layout(set = 1, binding = 3) uniform texture3D u_aerial_perspective_lut;

layout(scalar, set = 1, binding = 4) uniform atmosphere_settings {
    AtmosphereSettings settings;
} u_atmosphere_settings;

layout(scalar, set = 1, binding = 5) uniform directional_light {
    DirectionalLight light;
} u_directional_light;

layout(set = 1, binding = 6) uniform camera_buffer {
    CameraGPU camera;
} u_camera;

layout(push_constant) uniform push_constants {
    bool u_use_depth_buffer;
    bool u_use_sun_luminance;
};

void main() {
    const vec3 clip = vec3(vec2(vertex_uv) * 2.0f - 1.0f, -1.0f);
    vec4 unprojected = u_camera.camera.inv_projection * vec4(clip, 1.0f);
    unprojected.xyz /= unprojected.w;
    const vec3 rd = normalize(u_camera.camera.inv_view * vec4(unprojected.xyz, 0.0f)).xyz * vec3(1, -1, 1);
    
    const AtmosphereSettings atm = u_atmosphere_settings.settings;

    const vec3 pos = get_view_pos(u_camera.camera.position, atm.surface);
    const vec3 sun_dir = u_directional_light.light.direction * vec3(1, -1, 1);
    
    vec3 L = vec3(0.0f);
    
    const float depth = textureLod(sampler2D(u_depth, u_sampler), vertex_uv, 0).r;
    // draw the atmosphere behind the geometry
    if (!u_use_depth_buffer || depth == 0.0) {
        const float r = length(pos);
        if (r < atm.atmosphere) {
            const vec3 up = pos / r;
            const float mu = dot(rd, up);

            const vec3 right = normalize(cross(up, rd));
            const vec3 forward = normalize(cross(right, up));
            const float light_view_cos = normalize(vec2(dot(sun_dir, forward), dot(sun_dir, right))).x;

            const bool intersects_surface = intersect_sphere(pos, rd, vec3(0.0f), atm.surface).t != NO_HIT;

            const vec2 sky_view_uv = sky_view_uv_from_zen_view_cos(atm, intersects_surface, mu, light_view_cos, r);
            
            L = textureLod(sampler2D(u_sky_view_lut, u_sampler), sky_view_uv, 0).rgb;
            if (u_use_sun_luminance) {
                const vec2 transmittance_uv = transmittance_uv_from_r_mu(atm, r, dot(up, sun_dir));
                L += 
                    get_sun_luminance(pos, rd, sun_dir, atm.surface) *
                    textureLod(sampler2D(u_transmittance_lut, u_sampler), transmittance_uv, 0).rgb;
            }
                
            out_color = vec4(L, 1.0);
            return;
        }
    }
    if (!u_use_depth_buffer) {
        return;
    }
    
    // draw the aerial persective on top of the geometry
    const float linear_depth = -linearize_reverse_z(depth, u_camera.camera.near, u_camera.camera.far);
    float slice = aerial_perspective_km_to_slice(linear_depth);
    float weigth = 1.0f;
    if (slice < 0.5f) {
        // fade to 0 at 0 depth
        weigth = clamp(slice * 2.0f, 0.0f, 1.0f);
        slice = 0.5f;
    }
    const float aerial_slice_w = sqrt(slice / AERIAL_PERSPECTIVE_LUT_RES);
    
    const vec4 aerial_perspective =
        weigth * 
        textureLod(sampler3D(u_aerial_perspective_lut, u_sampler), vec3(vertex_uv, aerial_slice_w), 0);
    L = aerial_perspective.rgb;
    
    out_color = vec4(L, aerial_perspective.a);
}