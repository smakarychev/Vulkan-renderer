#version 460

#include "common.glsl"
#include "../pbr/pbr.glsl"
#include "../../light.glsl"

#extension GL_EXT_nonuniform_qualifier: enable

layout(location = 0) in flat uint vertex_material_id;
layout(location = 1) in vec3 vertex_position;
layout(location = 2) in vec3 vertex_normal;
layout(location = 3) in vec4 vertex_tangent;
layout(location = 4) in vec2 vertex_uv;

layout(location = 0) out vec4 out_color;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;

layout(set = 1, binding = 0) uniform camera {
    CameraGPU camera;
} u_camera;

layout(scalar, set = 1, binding = 4) readonly buffer directional_lights {
    DirectionalLight lights[];
} u_directional_lights;

layout(scalar, set = 1, binding = 5) readonly buffer point_lights {
    PointLight lights[];
} u_point_lights;

layout(scalar, set = 1, binding = 6) uniform lights_info {
    LightsInfo info;
} u_lights_info;

layout(std430, set = 2, binding = 0) readonly buffer material {
    Material materials[];
} u_materials;

@bindless
layout(set = 2, binding = 1) uniform texture2D u_textures[];

vec3 shade_directional_pbr(ShadeInfo shade_info, DirectionalLight light) {
    const vec3 light_dir = -light.direction;
    const vec3 radiance = light.color * light.intensity;

    const vec3 halfway_dir = normalize(light_dir + shade_info.view);

    const float n_dot_h = clamp(dot(shade_info.normal, halfway_dir), 0.0f, 1.0f);
    const float n_dot_l = clamp(dot(shade_info.normal, light_dir), 0.0f, 1.0f);
    const float h_dot_l = clamp(dot(halfway_dir, light_dir), 0.0f, 1.0f);

    const float D = d_ggx(n_dot_h, shade_info.alpha_roughness);
    const float V = v_smith_correlated(shade_info.n_dot_v, n_dot_l, shade_info.alpha_roughness);
    const vec3 F = fresnel_schlick(h_dot_l, shade_info.F0, shade_info.F90);

    const vec3 diffuse = (vec3(1.0f) - F) * shade_info.diffuse_color * PI_INV;
    const vec3 specular = D * V * F;

    return (specular + diffuse) * radiance * n_dot_l;
}

vec3 shade_pbr_point_light(ShadeInfo shade_info, PointLight light) {
    vec3 light_dir = light.position - shade_info.position;
    const float distance2 = dot(light_dir, light_dir);
    const float falloff = pbr_falloff(distance2, light.radius);
    light_dir = normalize(light_dir);

    const vec3 radiance = light.color * light.intensity * falloff;

    const vec3 halfway_dir = normalize(light_dir + shade_info.view);

    const float n_dot_h = clamp(dot(shade_info.normal, halfway_dir), 0.0f, 1.0f);
    const float n_dot_l = clamp(dot(shade_info.normal, light_dir), 0.0f, 1.0f);
    const float h_dot_l = clamp(dot(halfway_dir, light_dir), 0.0f, 1.0f);

    const float D = d_ggx(n_dot_h, shade_info.alpha_roughness);
    const float V = v_smith_correlated(shade_info.n_dot_v, n_dot_l, shade_info.alpha_roughness);
    const vec3 F = fresnel_schlick(h_dot_l, shade_info.F0, shade_info.F90);

    const vec3 diffuse = (vec3(1.0f) - F) * shade_info.diffuse_color * PI_INV;
    const vec3 specular = D * V * F;

    return (specular + diffuse) * radiance * n_dot_l;
}

void main() {
    out_color = vec4(normalize(vertex_position) * 0.5f + 0.5f, 1.0f);
    out_color = vec4(normalize(vertex_normal) * 0.5f + 0.5f, 1.0f);
    
    const Material material = u_materials.materials[vertex_material_id];

    vec4 albedo = material.albedo_color;
    albedo *= texture(nonuniformEXT(sampler2D(u_textures[material.albedo_texture_index], u_sampler)), vertex_uv);
    if (albedo.a < 0.5f)
        discard;

    vec3 normal = normalize(vertex_normal);
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
    shade_info.view = normalize(u_camera.camera.position - vertex_position);
    shade_info.n_dot_v = clamp(dot(shade_info.normal, shade_info.view), 0.0f, 1.0f);
    shade_info.perceptual_roughness = perceptual_roughness;
    shade_info.alpha_roughness = perceptual_roughness * perceptual_roughness;
    shade_info.metallic = metallic;
    shade_info.F0 = F0;
    shade_info.F90 = F90;
    shade_info.diffuse_color = diffuse_color;
    shade_info.specular_color = specular_color;
    shade_info.alpha = 1.0f; // unused

    vec3 color = vec3(0.0f);
    for (uint i = 0; i < u_lights_info.info.directional_light_count; i++)
        color += shade_directional_pbr(shade_info, u_directional_lights.lights[i]);
    for (uint i = 0; i < u_lights_info.info.point_light_count; i++)
        color += shade_pbr_point_light(shade_info, u_point_lights.lights[i]);
    color = tonemap(color, 2.0f);

    color += emissive + 0.1f * albedo.rgb;
    
    out_color = vec4(color, 1.0f);
}