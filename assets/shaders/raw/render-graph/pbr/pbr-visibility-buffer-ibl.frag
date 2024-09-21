// http://filmicworlds.com/blog/visibility-buffer-rendering-with-material-graphs/

#version 460

#include "common.glsl"
#include "pbr.glsl"

#extension GL_EXT_nonuniform_qualifier: require
#extension GL_EXT_samplerless_texture_functions: require
#extension GL_KHR_shader_subgroup_arithmetic: require

layout(constant_id = 0) const float MAX_REFLECTION_LOD = 5.0f;
layout(constant_id = 1) const bool USE_TILED_LIGHTING = false;
layout(constant_id = 2) const bool USE_CLUSTERED_LIGHTING = false;
layout(constant_id = 3) const bool USE_HYBRID_LIGHTING = false;

@immutable_sampler_nearest
layout(set = 0, binding = 0) uniform sampler u_sampler_visibility;

@immutable_sampler
layout(set = 0, binding = 1) uniform sampler u_sampler;

@immutable_sampler_clamp_edge
layout(set = 0, binding = 2) uniform sampler u_sampler_brdf;

@immutable_sampler_nearest_clamp_black
layout(set = 0, binding = 3) uniform sampler u_sampler_shadow;

layout(set = 1, binding = 0) uniform utexture2D u_visibility_texture;
layout(set = 1, binding = 1) uniform texture2D u_ssao_texture;
layout(set = 1, binding = 2) uniform textureCube u_irradiance_map;
layout(set = 1, binding = 3) uniform textureCube u_prefilter_map;
layout(set = 1, binding = 4) uniform texture2D u_brdf;

layout(set = 1, binding = 5) uniform camera_buffer {
    CameraGPU camera;
} u_camera;

layout(set = 1, binding = 6) uniform shading_settings {
    ShadingSettings settings;
} u_shading;

layout(scalar, set = 1, binding = 7) uniform directional_light {
    DirectionalLight light;
} u_directional_light;

layout(scalar, set = 1, binding = 8) readonly buffer point_light {
    PointLight lights[];
} u_point_lights;

layout(scalar, set = 1, binding = 9) uniform lights_info {
    LightsInfo info;
} u_lights_info;

layout(set = 1, binding = 10) readonly buffer clusters {
    Cluster clusters[];
} u_clusters;

layout(set = 1, binding = 11) readonly buffer tiles {
    Tile tiles[];
} u_tiles;

layout(scalar, set = 1, binding = 12) readonly buffer zbins {
    ZBin bins[];
} u_zbins;

layout(std430, set = 1, binding = 13) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

layout(std430, set = 1, binding = 14) readonly buffer objects_buffer {
    object_data objects[];
} u_objects;

layout(std430, set = 1, binding = 15) readonly buffer positions_buffer {
    Position positions[];
} u_positions;

layout(std430, set = 1, binding = 16) readonly buffer normals_buffer {
    Normal normals[];
} u_normals;

layout(std430, set = 1, binding = 17) readonly buffer tangents_buffer {
    Tangent tangents[];
} u_tangents;

layout(std430, set = 1, binding = 18) readonly buffer uvs_buffer {
    UV uvs[];
} u_uv;

layout(std430, set = 1, binding = 19) readonly buffer indices_buffer {
    uint8_t indices[];
} u_indices;

// shadow-related descriptors
layout(set = 1, binding = 20) uniform texture2DArray u_csm;
layout(scalar, set = 1, binding = 21) uniform csm_data_buffer {
    CSMData csm;
} u_csm_data;

layout(std430, set = 2, binding = 0) readonly buffer material_buffer{
    Material materials[];
} u_materials;

@bindless
layout(set = 2, binding = 1) uniform texture2D u_textures[];

layout(location = 0) in vec2 vertex_uv;
layout(location = 1) in vec2 vertex_position;

layout(location = 0) out vec4 out_color;

/* the content of this file depends on descriptor names */
#include "../shadows/shadows.glsl"
/* the content of this file depends on descriptor names */
#include "visiblity-buffer-utils.glsl"
/* the content of this file depends on descriptor names */
#include "pbr-shading.glsl"

uint hash(uint x) {
    uint state = x * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;

    return (word >> 22u) ^ word;
}

vec3 to_color(uint value) {
    return vec3(
        float(value & 255u) / 255.0f,
        float((value >> 8) & 255u) / 255.0f,
        float((value >> 16) & 255u) / 255.0f);
}

vec3 color_hash(uint x) {
    const uint hash_val = hash(x);

    return to_color(hash_val);
}

vec3 shade_pbr(ShadeInfo shade_info, float shadow, float ao) {
    vec3 Lo = vec3(0.0f);
    
    if (USE_TILED_LIGHTING) 
        Lo += shade_pbr_point_lights_tiled(shade_info);
    else if (USE_CLUSTERED_LIGHTING)
        Lo += shade_pbr_point_lights_clustered(shade_info);
    else if (USE_HYBRID_LIGHTING)
        Lo += shade_pbr_point_lights_hybrid(shade_info);
    
    Lo += shade_pbr_directional_light(shade_info, shadow);

    vec3 ambient = shade_pbr_ibl(shade_info) * u_shading.settings.environment_power;

    vec3 color = Lo + ambient;

    return color * ao;
}

void main() {
    uint visibility_packed = textureLod(usampler2D(u_visibility_texture, u_sampler_visibility), vertex_uv, 0).r;
    if (visibility_packed == (~0))
        return;

    const VisibilityInfo visibility_info = unpack_visibility(visibility_packed);
    const GBufferData gbuffer_data = get_gbuffer_data(visibility_info);

    float metallic = gbuffer_data.metallic_roughness.r;
    metallic = clamp(metallic, MIN_ROUGHNESS, 1.0f);
    const float perceptual_roughness = gbuffer_data.metallic_roughness.g;

    // todo: reflectance can be provided as a material parameter
    const float reflectance = 0.5f;
    const vec3 F0 = mix(vec3(0.16f * reflectance * reflectance), gbuffer_data.albedo, metallic);
    const vec3 F90 = vec3(1.0f);

    vec3 diffuse_color = (1.0f - metallic) * gbuffer_data.albedo * (vec3(1.0f) - F0);
    vec3 specular_color = F0;

    ShadeInfo shade_info;
    shade_info.position = gbuffer_data.position;
    shade_info.normal = gbuffer_data.normal;
    shade_info.view = normalize(u_camera.camera.position - shade_info.position);
    shade_info.n_dot_v = clamp(dot(shade_info.normal, shade_info.view), 0.0f, 1.0f);
    shade_info.perceptual_roughness = perceptual_roughness;
    shade_info.alpha_roughness = perceptual_roughness * perceptual_roughness;
    shade_info.metallic = metallic;
    shade_info.F0 = F0;
    shade_info.F90 = F90;
    shade_info.diffuse_color = diffuse_color;
    shade_info.specular_color = specular_color;
    shade_info.alpha = 1.0f; // unused
    shade_info.z_view = gbuffer_data.z_view;
    shade_info.depth = gbuffer_data.depth;
    
    const float ambient_occlusion = gbuffer_data.ao * textureLod(sampler2D(u_ssao_texture, u_sampler), vertex_uv, 0).r;

    const float shadow = shadow(gbuffer_data.position, gbuffer_data.flat_normal, u_directional_light.light.size);

    vec3 color;
    color = shade_pbr(shade_info, shadow, ambient_occlusion);
    color = tonemap(color, 2.0f);

    color += gbuffer_data.emissive;

    out_color = vec4(color, 1.0);
}