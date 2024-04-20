// http://filmicworlds.com/blog/visibility-buffer-rendering-with-material-graphs/

#version 460

#include "common.glsl"
#include "pbr.glsl"

#extension GL_EXT_nonuniform_qualifier: require
#extension GL_EXT_samplerless_texture_functions: require

layout(constant_id = 0) const float MAX_REFLECTION_LOD = 5.0f;

@immutable_sampler_nearest
layout(set = 0, binding = 0) uniform sampler u_sampler_visibility;

@immutable_sampler
layout(set = 0, binding = 1) uniform sampler u_sampler;

@immutable_sampler_clamp_edge
layout(set = 0, binding = 2) uniform sampler u_sampler_brdf;

@immutable_sampler_clamp_black
layout(set = 0, binding = 3) uniform sampler u_sampler_shadow;

layout(set = 1, binding = 0) uniform utexture2D u_visibility_texture;
layout(set = 1, binding = 1) uniform texture2D u_ssao_texture;
layout(set = 1, binding = 2) uniform textureCube u_irradiance_map;
layout(set = 1, binding = 3) uniform textureCube u_prefilter_map;
layout(set = 1, binding = 4) uniform texture2D u_brdf;

layout(set = 1, binding = 5) uniform camera_buffer {
    CameraGPU camera;
} u_camera;

layout(std430, set = 1, binding = 6) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

layout(std430, set = 1, binding = 7) readonly buffer objects_buffer {
    object_data objects[];
} u_objects;

layout(std430, set = 1, binding = 8) readonly buffer positions_buffer {
    Position positions[];
} u_positions;

layout(std430, set = 1, binding = 9) readonly buffer normals_buffer {
    Normal normals[];
} u_normals;

layout(std430, set = 1, binding = 10) readonly buffer tangents_buffer {
    Tangent tangents[];
} u_tangents;

layout(std430, set = 1, binding = 11) readonly buffer uvs_buffer {
    UV uvs[];
} u_uv;

layout(std430, set = 1, binding = 12) readonly buffer indices_buffer {
    uint8_t indices[];
} u_indices;


// shadow-related descriptors
layout(set = 1, binding = 13) uniform texture2D u_directional_shadow_map;
layout(set = 1, binding = 14) uniform directional_shadow_matrix {
    mat4 view_projection;
} u_directional_shadow_transform;

layout(std430, set = 2, binding = 0) readonly buffer material_buffer{
    Material materials[];
} u_materials;

@bindless
layout(set = 2, binding = 1) uniform texture2D u_textures[];

layout(location = 0) in vec2 vertex_uv;
layout(location = 1) in vec2 vertex_position;

layout(location = 0) out vec4 out_color;

uvec3 get_indices(VisibilityInfo visibility_info) {
    IndirectCommand command = u_commands.commands[visibility_info.instance_id];
    uvec3 indices = uvec3(
    command.vertexOffset + uint(u_indices.indices[command.firstIndex + visibility_info.triangle_id * 3 + 0]),
    command.vertexOffset + uint(u_indices.indices[command.firstIndex + visibility_info.triangle_id * 3 + 1]),
    command.vertexOffset + uint(u_indices.indices[command.firstIndex + visibility_info.triangle_id * 3 + 2]));

    return indices;
}

Triangle get_triangle_local(uvec3 indices) {
    Triangle triangle;
    triangle.a = vec4_from_position(u_positions.positions[indices.x]);
    triangle.b = vec4_from_position(u_positions.positions[indices.y]);
    triangle.c = vec4_from_position(u_positions.positions[indices.z]);

    return triangle;
}

void transform_to_clip_space(inout Triangle triangle, VisibilityInfo visibility_info) {
    IndirectCommand command = u_commands.commands[visibility_info.instance_id];
    object_data object = u_objects.objects[command.render_object];
    triangle.a = u_camera.camera.view_projection * object.model * triangle.a;
    triangle.b = u_camera.camera.view_projection * object.model * triangle.b;
    triangle.c = u_camera.camera.view_projection * object.model * triangle.c;
}

mat3x2 get_uvs(uvec3 indices) {
    vec2 uv_a = vec2(u_uv.uvs[indices.x].u, u_uv.uvs[indices.x].v);
    vec2 uv_b = vec2(u_uv.uvs[indices.y].u, u_uv.uvs[indices.y].v);
    vec2 uv_c = vec2(u_uv.uvs[indices.z].u, u_uv.uvs[indices.z].v);

    return mat3x2(uv_a, uv_b, uv_c);
}

mat3 get_normals(uvec3 indices) {
    vec3 normal_a = vec3(u_normals.normals[indices.x].x, u_normals.normals[indices.x].y, u_normals.normals[indices.x].z);
    vec3 normal_b = vec3(u_normals.normals[indices.y].x, u_normals.normals[indices.y].y, u_normals.normals[indices.y].z);
    vec3 normal_c = vec3(u_normals.normals[indices.z].x, u_normals.normals[indices.z].y, u_normals.normals[indices.z].z);

    return mat3(normal_a, normal_b, normal_c);
}

mat3 get_tangents(uvec3 indices) {
    vec3 tangent_a = vec3(u_tangents.tangents[indices.x].x, u_tangents.tangents[indices.x].y, u_tangents.tangents[indices.x].z);
    vec3 tangent_b = vec3(u_tangents.tangents[indices.y].x, u_tangents.tangents[indices.y].y, u_tangents.tangents[indices.y].z);
    vec3 tangent_c = vec3(u_tangents.tangents[indices.z].x, u_tangents.tangents[indices.z].y, u_tangents.tangents[indices.z].z);

    return mat3(tangent_a, tangent_b, tangent_c);
}

void convert_to_world_space_normal(inout vec3 normal, VisibilityInfo visibility_info) {
    IndirectCommand command = u_commands.commands[visibility_info.instance_id];
    object_data object = u_objects.objects[command.render_object];

    normal = normalize((transpose(inverse(object.model)) * vec4(normal, 0.0f)).xyz);
}

void convert_to_world_space_normal_tangents(
        inout vec3 normal, vec3 normal_dx, vec3 normal_dy,
        inout vec3 tangent, vec3 tangent_dx, vec3 tangent_dy,
        inout vec3 bitangent, inout vec3 bitangent_dx, inout vec3 bitangent_dy, VisibilityInfo visibility_info) {
    IndirectCommand command = u_commands.commands[visibility_info.instance_id];
    object_data object = u_objects.objects[command.render_object];

    tangent = normalize((transpose(inverse(object.model)) * vec4(tangent, 0.0f)).xyz);
    normal = normalize((transpose(inverse(object.model)) * vec4(normal, 0.0f)).xyz);
    // re-orthogonalize
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    bitangent = normalize(cross(normal, tangent));

    bitangent_dx = cross(normal_dx, tangent) - cross(tangent_dx, normal);
    bitangent_dy = cross(normal_dy, tangent) - cross(tangent_dy, normal);
}

Material get_material(VisibilityInfo visibility_info) {
    IndirectCommand command = u_commands.commands[visibility_info.instance_id];
    uint object_index = command.render_object;
    Material material = u_materials.materials[object_index];

    return material;
}

struct InterpolationData {
    vec3 barycentric;
    vec3 ddx;
    vec3 ddy;
};

InterpolationData get_interpolation_data(Triangle triangle, vec2 screen_pos, vec2 resolution) {
    vec3 one_over_w = 1.0f / vec3(triangle.a.w, triangle.b.w, triangle.c.w);

    vec2 ndc_a = triangle.a.xy * one_over_w.x;
    vec2 ndc_b = triangle.b.xy * one_over_w.y;
    vec2 ndc_c = triangle.c.xy * one_over_w.z;

    float inverse_det = 1.0f / determinant(mat2(ndc_c - ndc_b, ndc_a - ndc_b));
    vec3 ddx = vec3(ndc_b.y - ndc_c.y, ndc_c.y - ndc_a.y, ndc_a.y - ndc_b.y) * inverse_det * one_over_w;
    vec3 ddy = vec3(ndc_c.x - ndc_b.x, ndc_a.x - ndc_c.x, ndc_b.x - ndc_a.x) * inverse_det * one_over_w;
    float ddx_sum = dot(ddx, vec3(1.0f, 1.0f, 1.0f));
    float ddy_sum = dot(ddy, vec3(1.0f, 1.0f, 1.0f));

    vec2 delta_vec = screen_pos - ndc_a;
    float interpolated_inv_w = one_over_w.x + delta_vec.x * ddx_sum + delta_vec.y * ddy_sum;
    float interpolated_w = 1.0f / interpolated_inv_w;

    vec3 lambda;
    lambda.x = interpolated_w * (one_over_w.x + delta_vec.x * ddx.x + delta_vec.y * ddy.x);
    lambda.y = interpolated_w * (0.0f         + delta_vec.x * ddx.y + delta_vec.y * ddy.y);
    lambda.z = interpolated_w * (0.0f         + delta_vec.x * ddx.z + delta_vec.y * ddy.z);
    // todo: isn't it better?
    //lambda.z = 1.0 - lambda.x - lambda.y; 

    ddx *= (2.0f / resolution.x);
    ddy *= (2.0f / resolution.y);
    ddx_sum *= (2.0f / resolution.x);
    ddy_sum *= (2.0f / resolution.y);

    float interpolated_w_ddx = 1.0f / (interpolated_inv_w + ddx_sum);
    float interpolated_w_ddy = 1.0f / (interpolated_inv_w + ddy_sum);

    ddx = interpolated_w_ddx * (lambda * interpolated_inv_w + ddx) - lambda;
    ddy = interpolated_w_ddy * (lambda * interpolated_inv_w + ddy) - lambda;

    InterpolationData interpolation_data;
    interpolation_data.barycentric = lambda;
    interpolation_data.ddx = ddx;
    interpolation_data.ddy = ddy;

    return interpolation_data;
}

float interpolate(InterpolationData interpolation, vec3 attribute_vec) {
    return dot(attribute_vec, interpolation.barycentric);
}

float interpolate(InterpolationData interpolation, float a, float b, float c) {
    return interpolate(interpolation, vec3(a, b, c));
}

vec3 interpolate_with_derivatives(InterpolationData interpolation, vec3 attribute_vec) {
    vec3 result;
    result[0] = dot(attribute_vec, interpolation.barycentric);
    result[1] = dot(attribute_vec, interpolation.ddx);
    result[2] = dot(attribute_vec, interpolation.ddy);

    return result;
}

mat3x2 interpolate_with_derivatives_2d(InterpolationData interpolation, mat3x2 attributes) {
    vec3 attribute_x = vec3(attributes[0].x, attributes[1].x, attributes[2].x);
    vec3 attribute_y = vec3(attributes[0].y, attributes[1].y, attributes[2].y);

    vec3 attribute_x_interpolated = interpolate_with_derivatives(interpolation, attribute_x);
    vec3 attribute_y_interpolated = interpolate_with_derivatives(interpolation, attribute_y);

    float ax = attribute_x_interpolated[0];
    float ay = attribute_y_interpolated[0];

    float addxx = attribute_x_interpolated[1];
    float addxy = attribute_y_interpolated[1];

    float addyx = attribute_x_interpolated[2];
    float addyy = attribute_y_interpolated[2];

    return mat3x2(vec2(ax, ay), vec2(addxx, addxy), vec2(addyx, addyy));
}

vec3 interpolate_3d(InterpolationData interpolation, mat3 attributes) {
    vec3 attribute_x = vec3(attributes[0].x, attributes[1].x, attributes[2].x);
    vec3 attribute_y = vec3(attributes[0].y, attributes[1].y, attributes[2].y);
    vec3 attribute_z = vec3(attributes[0].z, attributes[1].z, attributes[2].z);

    float attribute_x_interpolated = interpolate(interpolation, attribute_x);
    float attribute_y_interpolated = interpolate(interpolation, attribute_y);
    float attribute_z_interpolated = interpolate(interpolation, attribute_z);

    return vec3(attribute_x_interpolated, attribute_y_interpolated, attribute_z_interpolated);
}

mat3 interpolate_with_derivatives_3d(InterpolationData interpolation, mat3 attributes) {
    vec3 attribute_x = vec3(attributes[0].x, attributes[1].x, attributes[2].x);
    vec3 attribute_y = vec3(attributes[0].y, attributes[1].y, attributes[2].y);
    vec3 attribute_z = vec3(attributes[0].z, attributes[1].z, attributes[2].z);

    vec3 attribute_x_interpolated = interpolate_with_derivatives(interpolation, attribute_x);
    vec3 attribute_y_interpolated = interpolate_with_derivatives(interpolation, attribute_y);
    vec3 attribute_z_interpolated = interpolate_with_derivatives(interpolation, attribute_z);

    float ax = attribute_x_interpolated[0];
    float ay = attribute_y_interpolated[0];
    float az = attribute_z_interpolated[0];

    float addxx = attribute_x_interpolated[1];
    float addxy = attribute_y_interpolated[1];
    float addxz = attribute_z_interpolated[1];

    float addyx = attribute_x_interpolated[2];
    float addyy = attribute_y_interpolated[2];
    float addyz = attribute_z_interpolated[2];

    return mat3(vec3(ax, ay, az), vec3(addxx, addxy, addxz), vec3(addyx, addyy, addyz));
}

vec3 shade_pbr_lights(ShadeInfo shade_info) {
    const vec3 light_dir = -normalize(vec3(-0.1f, -0.1f, -0.1f));
    const vec3 radiance = vec3(1.0f);
    
    const vec3 halfway_dir = normalize(light_dir + shade_info.view);

    const  float n_dot_h = clamp(dot(shade_info.normal, halfway_dir), 0.0f, 1.0f);
    const  float n_dot_l = clamp(dot(shade_info.normal, light_dir), 0.0f, 1.0f);
    const  float h_dot_l = clamp(dot(halfway_dir, light_dir), 0.0f, 1.0f);

    const float D = d_ggx(n_dot_h, shade_info.alpha_roughness);
    const float V = v_smith_correlated(shade_info.n_dot_v, n_dot_l, shade_info.alpha_roughness);
    const vec3 F = fresnel_schlick(h_dot_l, shade_info.F0, shade_info.F90);
    
    const vec3 diffuse = (vec3(1.0f) - F) * shade_info.diffuse_color * PI_INV;
    const vec3 specular = D * V * F;
    
    const vec3 Lo = (specular + diffuse) * radiance * n_dot_l;
    
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
    
    const vec3 ambient = specular + diffuse;
    
    return ambient;
}

vec3 shade_pbr(ShadeInfo shade_info) {
    const  float n_dot_v = clamp(dot(shade_info.normal, shade_info.view), 0.0f, 1.0f);

    vec3 Lo = vec3(0.0f);
    //Lo = shade_pbr_lights(shade_info);

    vec3 ambient = shade_pbr_ibl(shade_info);

    return ambient + Lo;
}

// the interpolated data (does not actually come from gbuffer)
struct GBufferData {
    vec3 position;
    vec2 uv;
    vec2 uv_dx;
    vec2 uv_dy;
    vec3 normal;
    vec3 flat_normal;
    vec3 albedo;
    vec3 emissive;
    vec2 metallic_roughness;
    float ao;
};

GBufferData get_gbuffer_data(VisibilityInfo visibility_info) {
    const uvec3 indices = get_indices(visibility_info);
    Triangle triangle = get_triangle_local(indices);
    transform_to_clip_space(triangle, visibility_info);
    const InterpolationData interpolation_data = get_interpolation_data(triangle, vertex_position, u_camera.camera.resolution);

    const float w = dot(vec3(triangle.a.w, triangle.b.w, triangle.c.w), interpolation_data.barycentric);
    const float z = -w * u_camera.camera.projection[2][2] + u_camera.camera.projection[3][2];
    const vec3 position = (u_camera.camera.inv_view_projection * vec4(vertex_position * w, z, w)).xyz;

    const mat3x2 uvs = get_uvs(indices);
    const mat3x2 uvs_interpolated = interpolate_with_derivatives_2d(interpolation_data, uvs);
    const vec2 uv = vec2(uvs_interpolated[0][0], uvs_interpolated[0][1]);
    const vec2 uv_dx = vec2(uvs_interpolated[1][0], uvs_interpolated[1][1]);
    const vec2 uv_dy = vec2(uvs_interpolated[2][0], uvs_interpolated[2][1]);

    const Material material = get_material(visibility_info);

    vec3 albedo = material.albedo_color.rgb;
    albedo *= textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.albedo_texture_index], u_sampler)), uv, uv_dx, uv_dy).rgb;
    
    vec3 emissive = textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.emissive_texture_index], u_sampler)), uv, uv_dx, uv_dy).rgb;

    vec2 metallic_roughness = textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.metallic_roughness_texture_index], u_sampler)), uv, uv_dx, uv_dy).bg;
    
    float ao = textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.ambient_occlusion_texture_index], u_sampler)), uv, uv_dx, uv_dy).r;

    const mat3 normals = get_normals(indices);
    const mat3 normals_interpolated = interpolate_with_derivatives_3d(interpolation_data, normals);
    const vec3 flat_normal = vec3(normals_interpolated[0][0], normals_interpolated[0][1], normals_interpolated[0][2]);
    vec3 normal = flat_normal;
    const vec3 normal_dx = vec3(normals_interpolated[1][0], normals_interpolated[1][1], normals_interpolated[1][2]);
    const vec3 normal_dy = vec3(normals_interpolated[2][0], normals_interpolated[2][1], normals_interpolated[2][2]);

    const mat3 tangents = get_tangents(indices);
    const mat3 tangents_interpolated = interpolate_with_derivatives_3d(interpolation_data, tangents);
    vec3 tangent = vec3(tangents_interpolated[0][0], tangents_interpolated[0][1], tangents_interpolated[0][2]);
    const vec3 tangent_dx = vec3(tangents_interpolated[1][0], tangents_interpolated[1][1], tangents_interpolated[1][2]);
    const vec3 tangent_dy = vec3(tangents_interpolated[2][0], tangents_interpolated[2][1], tangents_interpolated[2][2]);
    vec3 bitangent;
    vec3 bitangent_dx;
    vec3 bitangent_dy;
    convert_to_world_space_normal_tangents(
        normal, normal_dx, normal_dy,
        tangent, tangent_dx, tangent_dy,
        bitangent, bitangent_dx, bitangent_dy,
        visibility_info);

    vec3 normal_from_map = textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.normal_texture_index], u_sampler)), uv, uv_dx, uv_dy).rgb;
    normal_from_map = normalize(normal_from_map * 2.0f - 1.0f);
    normal = tangent * normal_from_map.x + bitangent * normal_from_map.y + normal * normal_from_map.z;
    
    GBufferData data;
    data.position = position;
    data.uv = uv;
    data.uv_dx = uv_dx;
    data.uv_dy = uv_dy;
    data.normal = normal;
    data.flat_normal = flat_normal;
    data.albedo = albedo;
    data.emissive = emissive;
    data.metallic_roughness = vec2(material.metallic, material.roughness) * metallic_roughness;
    data.ao = ao;
    
    return data;
}

float sample_shadow(float projected_depth, vec2 uv, vec2 delta) {
    const float bias = 0.0005f;
    const float depth = textureLod(sampler2D(u_directional_shadow_map, u_sampler_shadow), uv + delta, 0).r;
    const float shadow = depth < projected_depth + bias ? 0.0f : 0.8f;
    
    return shadow;
}

float shadow(vec3 position, vec3 normal) {

    const ivec2 shadow_size = textureSize(u_directional_shadow_map, 0);
    const float scale = 1.5f;
    const vec2 delta = vec2(scale) / vec2(shadow_size);
    
    const vec4 shadow_local = u_directional_shadow_transform.view_projection * vec4(position, 1.0f);
    const vec3 ndc = shadow_local.xyz / shadow_local.w;
    vec2 uv = (ndc.xy * 0.5f) + 0.5f;
    
    if (ndc.z < 0.0f)
        return 0.0f;
    
    float shadow_factor = 0.0f;
    int samples_dim = 1;
    int samples_count = 0;
    for (int x = -samples_dim; x <= samples_dim; x++) {
        for (int y = -samples_dim; y <= samples_dim; y++) {
            shadow_factor += sample_shadow(ndc.z, uv, vec2(delta.x * x, delta.y * y));
            samples_count++;
        }
    }
    shadow_factor /= float(samples_count);

    return shadow_factor;
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
    
    vec3 color;
    color = shade_pbr(shade_info);
    color = tonemap(color, 2.0f);

    float ambient_occlusion = 1.0f;
    ambient_occlusion *= gbuffer_data.ao * textureLod(sampler2D(u_ssao_texture, u_sampler), vertex_uv, 0).r;
    
    const float shadow = shadow(gbuffer_data.position, gbuffer_data.flat_normal);
    color *= ambient_occlusion * (1.0f - shadow);
    
    color += gbuffer_data.emissive;
    
    out_color = vec4(color, 1.0);
    //out_color = vec4(vec3(shadow), 1.0);
}