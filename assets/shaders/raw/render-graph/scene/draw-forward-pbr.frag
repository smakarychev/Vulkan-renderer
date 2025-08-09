#version 460

#include "../pbr/common.glsl"
#include "../pbr/pbr.glsl"

#extension GL_EXT_nonuniform_qualifier: enable
#extension GL_KHR_shader_subgroup_arithmetic: enable

layout(constant_id = 0) const float MAX_REFLECTION_LOD = 5.0f;
layout(constant_id = 1) const bool USE_TILED_LIGHTING = false;
layout(constant_id = 2) const bool USE_CLUSTERED_LIGHTING = false;
layout(constant_id = 3) const bool USE_HYBRID_LIGHTING = false;

layout(location = 0) in flat uint vertex_material_id;
layout(location = 1) in vec3 vertex_position;
layout(location = 2) in vec3 vertex_normal;
layout(location = 3) in vec4 vertex_tangent;
layout(location = 4) in vec2 vertex_uv;
layout(location = 5) in float vertex_z_view;

layout(location = 0) out vec4 out_color;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;

@immutable_sampler_clamp_edge
layout(set = 0, binding = 1) uniform sampler u_sampler_ce;

@immutable_sampler_shadow
layout(set = 0, binding = 2) uniform sampler u_sampler_shadow;

layout(scalar, set = 1, binding = 0) uniform view_info {
    ViewInfo view;
} u_view_info;

layout(scalar, set = 1, binding = 4) readonly buffer directional_lights {
    DirectionalLight lights[];
} u_directional_lights;

layout(scalar, set = 1, binding = 5) readonly buffer point_lights {
    PointLight lights[];
} u_point_lights;

layout(scalar, set = 1, binding = 6) uniform lights_info {
    LightsInfo info;
} u_lights_info;

layout(set = 1, binding = 7) readonly buffer clusters {
    Cluster clusters[];
} u_clusters;

layout(set = 1, binding = 8) readonly buffer tiles {
    Tile tiles[];
} u_tiles;

layout(scalar, set = 1, binding = 9) readonly buffer zbins {
    ZBin bins[];
} u_zbins;

layout(set = 1, binding = 10) uniform texture2D u_ssao_texture;

layout(set = 1, binding = 11) uniform irradiance_sh {
    SH9Irradiance sh;
} u_irradiance_SH;
layout(set = 1, binding = 12) uniform textureCube u_prefilter_map;
layout(set = 1, binding = 13) uniform texture2D u_brdf;

layout(set = 1, binding = 14) uniform texture2DArray u_csm;
layout(scalar, set = 1, binding = 15) uniform csm_data_buffer {
    CSMData csm;
} u_csm_data;

layout(std430, set = 2, binding = 0) readonly buffer material {
    Material materials[];
} u_materials;

@bindless
layout(set = 2, binding = 1) uniform texture2D u_textures[];

/* the content of this file depends on descriptor names */
#include "../shadows/shadows.glsl"
/* the content of this file depends on descriptor names */
#include "../pbr/pbr-shading.glsl"
#include "../atmosphere/atmosphere-functions.glsl"

vec3 shade_pbr(ShadeInfo shade_info, float shadow, float ao, vec2 frame_uv, vec3 transmittance) {
    vec3 Lo = vec3(0.0f);

    if (USE_TILED_LIGHTING)
        Lo += shade_pbr_point_lights_tiled(frame_uv, shade_info);
    else if (USE_CLUSTERED_LIGHTING)
        Lo += shade_pbr_point_lights_clustered(frame_uv, shade_info);
    else if (USE_HYBRID_LIGHTING)
        Lo += shade_pbr_point_lights_hybrid(frame_uv, shade_info);

    Lo += shade_pbr_directional_lights(shade_info, shadow, transmittance);

    const vec3 ambient = shade_pbr_ibl(shade_info) * u_view_info.view.environment_power;

    vec3 color = Lo + ambient;

    return color * ao;
}

vec3 get_transmittance() {
    const vec3 atm_pos = get_view_pos(u_view_info.view.position, u_view_info.view.surface);
    const vec3 sun_dir = u_directional_lights.lights[0].direction * vec3(1, -1, 1);
    const float r = length(atm_pos);
    const vec3 up = atm_pos / r;

    const bool intersects_surface = intersect_sphere(atm_pos, sun_dir, vec3(0.0f) + PLANET_RADIUS_OFFSET_KM * up, u_view_info.view.surface).depth != 0.0f;
    if (intersects_surface) {
        return vec3(0.0f);
    }

    vec3 transmittance = vec3(0.0f);
    const float mu = dot(up, sun_dir);
    const vec2 transmittance_uv = transmittance_uv_from_r_mu(u_view_info.view, r, dot(up, sun_dir));
    transmittance = textureLod(nonuniformEXT(sampler2D(u_textures[
    u_view_info.view.transmittance_lut], u_sampler_ce)), transmittance_uv, 0).rgb;

    return transmittance;
}

void main() {
    const Material material = u_materials.materials[vertex_material_id];

    vec4 albedo = material.albedo_color;
    albedo *= texture(nonuniformEXT(sampler2D(u_textures[material.albedo_texture_index], u_sampler)), vertex_uv);
    if (albedo.a < 0.5f)
        discard;

    vec3 normal = normalize(vertex_normal);
    const vec3 flat_normal = normal;
    vec3 tangent = normalize(vertex_tangent.xyz);
    // re-orthogonalize
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    const vec3 bitangent = normalize(cross(normal, tangent) * vertex_tangent.w);
    vec3 normal_from_map = texture(nonuniformEXT(sampler2D(u_textures[
        material.normal_texture_index], u_sampler)), vertex_uv).rgb;
    normal_from_map = normalize(normal_from_map * 2.0f - 1.0f);
    normal = tangent * normal_from_map.x + bitangent * normal_from_map.y + normal * normal_from_map.z;

    const vec3 emissive = texture(nonuniformEXT(sampler2D(u_textures[
        material.emissive_texture_index], u_sampler)), vertex_uv).rgb;

    const vec2 metallic_roughness = texture(nonuniformEXT(sampler2D(u_textures[
        material.metallic_roughness_texture_index], u_sampler)), vertex_uv).bg;

    float metallic = metallic_roughness.r * material.metallic;
    const float perceptual_roughness = clamp(metallic_roughness.g * material.roughness, MIN_ROUGHNESS, 1.0f);

    const float reflectance = 0.5f;
    const vec3 F0 = mix(vec3(0.16f * reflectance * reflectance), albedo.rgb, metallic);
    const vec3 F90 = vec3(1.0f);

    vec3 diffuse_color = (1.0f - metallic) * albedo.rgb * (vec3(1.0f) - F0);
    vec3 specular_color = F0;

    ShadeInfo shade_info;
    shade_info.position = vertex_position;
    shade_info.normal = normal;
    shade_info.view = normalize(u_view_info.view.position - vertex_position);
    shade_info.n_dot_v = clamp(dot(shade_info.normal, shade_info.view), 0.0f, 1.0f);
    shade_info.perceptual_roughness = perceptual_roughness;
    shade_info.alpha_roughness = perceptual_roughness * perceptual_roughness;
    shade_info.metallic = metallic;
    shade_info.F0 = F0;
    shade_info.F90 = F90;
    shade_info.diffuse_color = diffuse_color;
    shade_info.specular_color = specular_color;
    shade_info.alpha = 1.0f; // unused
    shade_info.depth = gl_FragCoord.z;

    const vec2 frame_uv = gl_FragCoord.xy / u_view_info.view.resolution;
    const float ambient_occlusion = textureLod(sampler2D(u_ssao_texture, u_sampler), frame_uv, 0).r;

    const float shadow = shadow(vertex_position, flat_normal, u_directional_lights.lights[0].direction, u_directional_lights.lights[0].size,
        vertex_z_view);
    
    const vec3 transmittance = get_transmittance();
    vec3 color = shade_pbr(shade_info, shadow, ambient_occlusion, frame_uv, transmittance);
    
    color = tonemap(color, 2.0f);
    color += emissive;
    
    out_color = vec4(color, 1.0f);
    #ifdef TEST 
    out_color = vec4(1.3f, 0.6f, 0.4f, 1.0f);
    #endif
}