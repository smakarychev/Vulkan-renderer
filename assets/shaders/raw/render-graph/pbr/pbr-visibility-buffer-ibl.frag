// http://filmicworlds.com/blog/visibility-buffer-rendering-with-material-graphs/

#version 460

#include "common.shader_header"
#include "pbr.shader_header"

#extension GL_EXT_nonuniform_qualifier : require

layout(constant_id = 0) const float MAX_REFLECTION_LOD = 5.0f;

@immutable_sampler_nearest
layout(set = 0, binding = 0) uniform sampler u_sampler_visibility;

@immutable_sampler
layout(set = 0, binding = 1) uniform sampler u_sampler;

@immutable_sampler_clamp_edge
layout(set = 0, binding = 2) uniform sampler u_sampler_clamp;

layout(set = 1, binding = 0) uniform utexture2D u_visibility_texture;
layout(set = 1, binding = 1) uniform texture2D u_ssao_texture;
layout(set = 1, binding = 2) uniform textureCube u_irradiance_map;
layout(set = 1, binding = 3) uniform textureCube u_prefilter_map;
layout(set = 1, binding = 4) uniform texture2D u_brdf;

layout(set = 1, binding = 5) uniform camera_buffer {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    mat4 view_projection_inverse;
    vec4 camera_position;
    vec2 resolution;
    float frustum_near;
    float frustum_far;
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
    triangle.a = u_camera.view_projection * object.model * triangle.a;
    triangle.b = u_camera.view_projection * object.model * triangle.b;
    triangle.c = u_camera.view_projection * object.model * triangle.c;
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
inout vec3 bitangent, inout vec3 bitangent_dx, inout vec3 bitangent_dy,
VisibilityInfo visibility_info) {
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

struct ShadeInfo {
    vec3 position;
    vec3 normal;
    vec3 normal_dx;
    vec3 normal_dy;
    vec3 albedo;
    float metallic;
    float roughness;
    float ambient_occlusion;
    mat3 tbn_dx;
    mat3 tbn_dy;
    mat3 tbn;
};

vec3 shade(ShadeInfo shade_info) {
    // todo: remove upon adding actual lights support
    const vec3 light_dir = normalize(vec3(1.5, 1.5, 1.0));

    vec3 view_dir = normalize(u_camera.camera_position.xyz - shade_info.position);
    vec3 halfway_dir = normalize(light_dir + view_dir);

    float ambient_power = 0.05f;
    vec3 ambient = shade_info.albedo * ambient_power;

    float diffuse_power = max(0.0f, dot(shade_info.normal, light_dir));
    vec3 diffuse = shade_info.albedo * diffuse_power;

    float specular_power = pow(max(0.0f, dot(shade_info.normal, halfway_dir)), 128.0);
    vec3 specular = vec3(0.3) * specular_power;

    return ambient + diffuse + specular;
}

vec3 shade_pbr_lights(ShadeInfo shade_info, vec3 F0, vec3 view_dir, float n_dot_v, float roughness) {
    const vec3 light_dir = -normalize(vec3(-0.1f, -0.1f, -0.1f));
    const vec3 radiance = vec3(15.0f, 14.0f, 14.0f);
    
    const vec3 halfway_dir = normalize(light_dir + view_dir);

    const  float n_dot_h = clamp(dot(shade_info.normal, halfway_dir), 0.0f, 1.0f);
    const  float n_dot_l = clamp(dot(shade_info.normal, light_dir), 0.0f, 1.0f);
    const  float h_dot_l = clamp(dot(halfway_dir, light_dir), 0.0f, 1.0f);

    const float D = d_ggx(n_dot_h, roughness);
    const float V = v_smith_correlated(n_dot_v, n_dot_l, roughness);
    const vec3 F = fresnel_schlick(h_dot_l, F0);

    const vec3 Fr = D * V * F;
    const vec3 kd = (vec3(1.0f) - F) * (1.0f - shade_info.metallic);
    const vec3 Fd = shade_info.albedo / PI;
    
    const vec3 Lo = (Fr + kd * Fd) * radiance * n_dot_l;
    
    return Lo;
}

vec3 shade_pbr_ibl(ShadeInfo shade_info, vec3 F0, vec3 view_dir, float n_dot_v, float roughness) {
    const vec3 F = fresnel_schlick_roughness(n_dot_v, F0, roughness);
    const vec3 irradiance = textureLod(samplerCube(u_irradiance_map, u_sampler), shade_info.normal, 0).rgb;

    const vec3 R = reflect(-view_dir, shade_info.normal);
    const vec3 prefilteredColor =
        textureLod(samplerCube(u_prefilter_map, u_sampler), R, roughness * MAX_REFLECTION_LOD).rgb;
    const vec2 brdf = textureLod(sampler2D(u_brdf, u_sampler_clamp), vec2(n_dot_v, roughness), 0).rg;

    const vec3 Fr = prefilteredColor * (F * brdf.x + brdf.y);
    const vec3 kd = vec3(1.0f) - F;
    const vec3 Fd = irradiance * shade_info.albedo;

    const float ao = textureLod(sampler2D(u_ssao_texture, u_sampler), vertex_uv, 0).r * shade_info.ambient_occlusion;
    
    const vec3 ambient = (Fr + kd * Fd) * ao;
    
    return ambient;
}

vec3 shade_pbr(ShadeInfo shade_info) {
    const vec3 view_dir = normalize(u_camera.camera_position.xyz - shade_info.position);

    // remapping of perceptual roughness
    float roughness = shade_info.roughness * shade_info.roughness;
    const  float n_dot_v = clamp(dot(shade_info.normal, view_dir), 0.0f, 1.0f);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, shade_info.albedo, shade_info.metallic);
    
    vec3 Lo = vec3(0.0f);
    //Lo = shade_pbr_lights(shade_info, F0, view_dir, n_dot_v, roughness);

    vec3 ambient = shade_pbr_ibl(shade_info, F0, view_dir, n_dot_v, roughness);

    return ambient + Lo;
}

void main() {
    uint visibility_packed = textureLod(usampler2D(u_visibility_texture, u_sampler_visibility), vertex_uv, 0).r;
    if (visibility_packed == (~0)) {
        //out_color = vec4(0.01f, 0.01f, 0.01f, 1.0f);
        //out_color = vec4(0.215f, 0.22f, 0.22f, 1.0f);
        return;
    }

    VisibilityInfo visibility_info = unpack_visibility(visibility_packed);

    uvec3 indices = get_indices(visibility_info);

    Triangle triangle = get_triangle_local(indices);
    transform_to_clip_space(triangle, visibility_info);

    InterpolationData interpolation_data = get_interpolation_data(triangle, vertex_position, u_camera.resolution);

    vec3 one_over_w = 1.0f / vec3(triangle.a.w, triangle.b.w, triangle.c.w);
    float w = 1.0f / dot(one_over_w, interpolation_data.barycentric);
    float z = -w * u_camera.projection[2][2] + u_camera.projection[3][2];
    vec3 position = (u_camera.view_projection_inverse * vec4(vertex_position * w, z, w)).xyz;

    mat3x2 uvs = get_uvs(indices);
    mat3x2 uvs_interpolated = interpolate_with_derivatives_2d(interpolation_data, uvs);
    vec2 uv = vec2(uvs_interpolated[0][0], uvs_interpolated[0][1]);
    vec2 uv_dx = vec2(uvs_interpolated[1][0], uvs_interpolated[1][1]);
    vec2 uv_dy = vec2(uvs_interpolated[2][0], uvs_interpolated[2][1]);

    Material material = get_material(visibility_info);
    vec3 albedo = material.albedo_color.rgb;
    albedo *= textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.albedo_texture_index], u_sampler)), uv, uv_dx, uv_dy).rgb;

    mat3 normals = get_normals(indices);
    mat3 normals_interpolated = interpolate_with_derivatives_3d(interpolation_data, normals);
    vec3 normal = vec3(normals_interpolated[0][0], normals_interpolated[0][1], normals_interpolated[0][2]);
    vec3 normal_dx = vec3(normals_interpolated[1][0], normals_interpolated[1][1], normals_interpolated[1][2]);
    vec3 normal_dy = vec3(normals_interpolated[2][0], normals_interpolated[2][1], normals_interpolated[2][2]);

    mat3 tangents = get_tangents(indices);
    mat3 tangents_interpolated = interpolate_with_derivatives_3d(interpolation_data, tangents);
    vec3 tangent = vec3(tangents_interpolated[0][0], tangents_interpolated[0][1], tangents_interpolated[0][2]);
    vec3 tangent_dx = vec3(tangents_interpolated[1][0], tangents_interpolated[1][1], tangents_interpolated[1][2]);
    vec3 tangent_dy = vec3(tangents_interpolated[2][0], tangents_interpolated[2][1], tangents_interpolated[2][2]);
    vec3 bi_tangent;
    vec3 bi_tangent_dx;
    vec3 bi_tangent_dy;
    convert_to_world_space_normal_tangents(
        normal, normal_dx, normal_dy,
        tangent, tangent_dx, tangent_dy,
        bi_tangent, bi_tangent_dx, bi_tangent_dy,
        visibility_info);
    
    vec3 normal_from_map = textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.normal_texture_index], u_sampler)), uv, uv_dx, uv_dy).rgb;
    normal_from_map = normalize(normal_from_map * 2.0f - 1.0f);
    normal = tangent * normal_from_map.x + bi_tangent * normal_from_map.y + normal * normal_from_map.z;

    float metallic = material.metallic;
    float roughness = material.roughness;
    vec3 metallic_roughness = textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.metallic_roughness_texture_index], u_sampler)), uv, uv_dx, uv_dy).rgb;

    metallic *= metallic_roughness.b;
    roughness *= metallic_roughness.g;

    float ambient_occlusion = 1.0;
    ambient_occlusion *= textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.ambient_occlusion_texture_index], u_sampler)), uv, uv_dx, uv_dy).r;


    ShadeInfo shade_info;
    shade_info.albedo = albedo;
    shade_info.position = position;
    shade_info.normal = normal;
    shade_info.normal_dx = normal_dx;
    shade_info.normal_dy = normal_dy;
    shade_info.metallic = metallic;
    shade_info.roughness = roughness;
    shade_info.ambient_occlusion = ambient_occlusion;
    shade_info.tbn_dx = mat3(tangent_dx, bi_tangent_dx, normal_dx);
    shade_info.tbn_dy = mat3(tangent_dy, bi_tangent_dy, normal_dy);
    shade_info.tbn = mat3(tangent, bi_tangent, normal);

    vec3 color;
    color = shade_pbr(shade_info);
    color = tonemap(color, 1.0f);

    out_color = vec4(color, 1.0);
}