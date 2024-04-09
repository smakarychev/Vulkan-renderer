#version 460

#include "common.glsl"
#include "pbr.glsl"

#extension GL_EXT_nonuniform_qualifier: require

layout(location = 0) in flat uint vertex_object_index;
layout(location = 1) in vec3 vertex_position;
layout(location = 2) in vec3 vertex_normal;
layout(location = 3) in vec3 vertex_tangent;
layout(location = 4) in vec2 vertex_uv;

layout(constant_id = 0) const float MAX_REFLECTION_LOD = 5.0f;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;

@immutable_sampler_clamp_edge
layout(set = 0, binding = 1) uniform sampler u_sampler_brdf;

layout(set = 1, binding = 0) uniform camera_buffer {
    CameraGPU camera;
} u_camera;

layout(std430, set = 1, binding = 6) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

layout(set = 1, binding = 7) uniform textureCube u_irradiance_map;
layout(set = 1, binding = 8) uniform textureCube u_prefilter_map;
layout(set = 1, binding = 9) uniform texture2D u_brdf;

layout(std430, set = 2, binding = 0) readonly buffer material_buffer{
    Material materials[];
} u_materials;

@bindless
layout(set = 2, binding = 1) uniform texture2D u_textures[];

layout(location = 0) out vec4 out_color;

vec3 shade_pbr_lights(ShadeInfo shade_info) {
    const vec3 light_dir = -normalize(vec3(-0.1f, -0.1f, -0.1f));
    const vec3 radiance = vec3(1.0f);

    const vec3 halfway_dir = normalize(light_dir + shade_info.view);

    const float n_dot_h = clamp(dot(shade_info.normal, halfway_dir), 0.0f, 1.0f);
    const float n_dot_l = clamp(dot(shade_info.normal, light_dir), 0.0f, 1.0f);
    const float h_dot_l = clamp(dot(halfway_dir, light_dir), 0.0f, 1.0f);

    const float D = d_ggx(n_dot_h, shade_info.alpha_roughness);
    const float V = v_smith_correlated(shade_info.n_dot_v, n_dot_l, shade_info.alpha_roughness);
    const vec3 F = fresnel_schlick(h_dot_l, shade_info.F0, shade_info.F90);

    const vec3 diffuse = (vec3(1.0f) - F) * shade_info.diffuse_color * PI_INV;
    const vec3 specular = D * V * F;

    const vec3 Lo = (specular + diffuse * shade_info.alpha) * radiance * n_dot_l;

    return Lo;
}

vec3 shade_pbr_ibl(ShadeInfo shade_info) {
    const vec3 R = reflect(-shade_info.view, shade_info.normal);
    const vec3 irradiance = textureLod(samplerCube(u_irradiance_map, u_sampler), shade_info.normal, 0).rgb;

    const float lod = shade_info.perceptual_roughness * MAX_REFLECTION_LOD;
    const vec3 prefiltered = textureLod(samplerCube(u_prefilter_map, u_sampler), R, lod).rgb;
    const vec2 brdf = textureLod(sampler2D(u_brdf, u_sampler_brdf),
    vec2(shade_info.n_dot_v, shade_info.perceptual_roughness), 0).rg;

    const vec3 diffuse = irradiance * shade_info.diffuse_color;
    const vec3 specular = (shade_info.F0 * brdf.x + shade_info.F90 * brdf.y) * prefiltered;

    const vec3 ambient = specular + diffuse * shade_info.alpha;

    return ambient;
}

vec3 shade_pbr(ShadeInfo shade_info) {
    const  float n_dot_v = clamp(dot(shade_info.normal, shade_info.view), 0.0f, 1.0f);

    vec3 Lo = vec3(0.0f);
    //Lo = shade_pbr_lights(shade_info);

    vec3 ambient = shade_pbr_ibl(shade_info);

    return ambient + Lo;
}

void main() {
    const Material material = u_materials.materials[vertex_object_index];

    vec4 albedo = material.albedo_color;
    albedo *= texture(nonuniformEXT(sampler2D(u_textures[material.albedo_texture_index], u_sampler)), vertex_uv);

    vec3 normal = normalize(vertex_normal);
    vec3 tangent = normalize(vertex_tangent);
    // re-orthogonalize
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    const vec3 bitangent = normalize(cross(normal, tangent));
    vec3 normal_from_map = texture(nonuniformEXT(sampler2D(u_textures[
        material.normal_texture_index], u_sampler)), vertex_uv).rgb;
    normal_from_map = normalize(normal_from_map * 2.0f - 1.0f);
    normal = tangent * normal_from_map.x + bitangent * normal_from_map.y + normal * normal_from_map.z;

    vec3 emissive = texture(nonuniformEXT(sampler2D(u_textures[
        material.emissive_texture_index], u_sampler)), vertex_uv).rgb;

    vec2 metallic_roughness = texture(nonuniformEXT(sampler2D(u_textures[
        material.metallic_roughness_texture_index], u_sampler)), vertex_uv).bg;

    float metallic = metallic_roughness.r;
    metallic = clamp(metallic, MIN_ROUGHNESS, 1.0f);
    const float perceptual_roughness = metallic_roughness.g;

    // todo: reflectance can be provided as a material parameter
    const float reflectance = 0.5f;
    const vec3 F0 = mix(vec3(0.16f * reflectance * reflectance), albedo.rgb, metallic);
    const vec3 F90 = vec3(1.0f);

    vec3 diffuse_color = (1.0f - metallic) * albedo.rgb * (vec3(1.0f) - F0);
    vec3 specular_color = F0;

    ShadeInfo shade_info;
    shade_info.position = vertex_position;
    shade_info.normal = normal;
    shade_info.view = normalize(u_camera.camera.position - vertex_position);
    shade_info.n_dot_v = clamp(dot(shade_info.normal, shade_info.view), 0.0f, 1.0f);
    shade_info.perceptual_roughness = perceptual_roughness;
    shade_info.alpha_roughness = perceptual_roughness * perceptual_roughness;
    shade_info.metallic = metallic;
    shade_info.F0 = F0;
    shade_info.F90 = F90;
    shade_info.diffuse_color = diffuse_color;
    shade_info.specular_color = specular_color;
    shade_info.alpha = albedo.a;

    vec3 color;
    color = shade_pbr(shade_info);
    color = tonemap(color, 2.0f);

    color += emissive;

    out_color = vec4(color, albedo.a);
}