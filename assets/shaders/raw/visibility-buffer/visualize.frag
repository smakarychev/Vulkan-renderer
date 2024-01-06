#version 460

#include "common.shader_header"

#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform usampler2D u_visibility_texture;

@dynamic
layout(set = 0, binding = 1) uniform camera_buffer {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    mat4 view_projection_inverse;
    vec2 window_size;
    float frustum_near;
    float frustum_far;
} u_camera_buffer;

layout(std430, set = 0, binding = 2) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_command_buffer;

layout(std430, set = 0, binding = 3) readonly buffer object_buffer {
    object_data objects[];
} u_object_buffer;

layout(std430, set = 0, binding = 4) readonly buffer positions_buffer {
    Position positions[];
} u_positions_buffer;

layout(std430, set = 0, binding = 5) readonly buffer uv_buffer {
    UV uvs[];
} u_uv_buffer;

layout(std430, set = 0, binding = 6) readonly buffer indices_buffer {
    uint8_t indices[];
} u_indices_buffer;

layout(std430, set = 0, binding = 7) readonly buffer material_buffer{
    Material materials[];
} u_material_buffer;

@immutable_sampler
layout(set = 1, binding = 0) uniform sampler u_sampler;

@bindless
layout(set = 1, binding = 1) uniform texture2D u_textures[];

layout(location = 0) in vec2 vert_uv;
layout (location = 1) in vec2 vert_position;

layout(location = 0) out vec4 out_color;

uvec3 get_indices(VisibilityInfo visibility_info) {
    IndirectCommand command = u_command_buffer.commands[visibility_info.instance_id];
    uvec3 indices = uvec3(
        command.vertexOffset + uint(u_indices_buffer.indices[command.firstIndex + visibility_info.triangle_id * 3 + 0]),
        command.vertexOffset + uint(u_indices_buffer.indices[command.firstIndex + visibility_info.triangle_id * 3 + 1]),
        command.vertexOffset + uint(u_indices_buffer.indices[command.firstIndex + visibility_info.triangle_id * 3 + 2]));
    
    return indices;
}

Triangle get_triangle_local(uvec3 indices) {
    Triangle triangle;
    triangle.a = vec4_from_position(u_positions_buffer.positions[indices.x]);
    triangle.b = vec4_from_position(u_positions_buffer.positions[indices.y]);
    triangle.c = vec4_from_position(u_positions_buffer.positions[indices.z]);
    
    return triangle;
}

void transform_to_clip_space(inout Triangle triangle, VisibilityInfo visibility_info) {
    IndirectCommand command = u_command_buffer.commands[visibility_info.instance_id];
    object_data object = u_object_buffer.objects[command.render_object];
    triangle.a = u_camera_buffer.view_projection * object.model * triangle.a;
    triangle.b = u_camera_buffer.view_projection * object.model * triangle.b;
    triangle.c = u_camera_buffer.view_projection * object.model * triangle.c;
}

mat3x2 get_uvs(uvec3 indices) {
    vec2 uv_a = vec2(u_uv_buffer.uvs[indices.x].u, u_uv_buffer.uvs[indices.x].v); 
    vec2 uv_b = vec2(u_uv_buffer.uvs[indices.y].u, u_uv_buffer.uvs[indices.y].v); 
    vec2 uv_c = vec2(u_uv_buffer.uvs[indices.z].u, u_uv_buffer.uvs[indices.z].v); 
    
    return mat3x2(uv_a, uv_b, uv_c);
}

Material get_material(VisibilityInfo visibility_info) {
    IndirectCommand command = u_command_buffer.commands[visibility_info.instance_id];
    uint object_index = command.render_object;
    Material material = u_material_buffer.materials[object_index];
    
    return material;
}

struct InterpolationData {
    vec3 barycentric;
    vec3 ddx;
    vec3 ddy;
};

InterpolationData get_interpolation_data(Triangle triangle, vec2 screen_pos, vec2 window_size) {
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

    ddx *= (2.0f / window_size.x);
    ddy *= (2.0f / window_size.y);
    ddx_sum *= (2.0f / window_size.x);
    ddy_sum *= (2.0f / window_size.y);

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

mat3x2 interpolate_2d(InterpolationData interpolation, mat3x2 attributes)
{
    vec3 attribute_x = vec3(attributes[0].x, attributes[1].x, attributes[2].x);
    vec3 attribute_y = vec3(attributes[0].y, attributes[1].y, attributes[2].y);
    
    vec3 attribute_x_interpolated = interpolate_with_derivatives(interpolation, attribute_x);
    vec3 attribute_y_interpolated = interpolate_with_derivatives(interpolation, attribute_y);
    
    float ux = attribute_x_interpolated[0];
    float uy = attribute_y_interpolated[0];
    
    float uddxx = attribute_x_interpolated[1];
    float uddxy = attribute_y_interpolated[1];
    
    float uddyx = attribute_x_interpolated[2];
    float uddyy = attribute_y_interpolated[2];
    
    return mat3x2(vec2(ux, uy), vec2(uddxx, uddxy), vec2(uddyx, uddyy));    
}

void main() {
    uint visibility_packed = textureLod(u_visibility_texture, vert_uv, 0).r;
    if (visibility_packed == (~0)) {
        out_color = vec4(0.05f, 0.05f, 0.05f, 1.0f);
        return;
    }
    
    VisibilityInfo visibility_info = unpack_visibility(visibility_packed);
    
    uvec3 indices = get_indices(visibility_info);

    Triangle triangle = get_triangle_local(indices);
    transform_to_clip_space(triangle, visibility_info);

    InterpolationData interpolation_data = get_interpolation_data(triangle, vert_position, u_camera_buffer.window_size);

    vec3 one_over_w = 1.0f / vec3(triangle.a.w, triangle.b.w, triangle.c.w);
    float w = 1.0f / dot(one_over_w, interpolation_data.barycentric);
    float z = -w * u_camera_buffer.projection[2][2] + u_camera_buffer.projection[3][2];
    vec3 position = (u_camera_buffer.view_projection_inverse * vec4(vert_position * w, z, w)).xyz;
    
    mat3x2 uvs = get_uvs(indices);
    mat3x2 uvs_interpolated = interpolate_2d(interpolation_data, uvs);
    
    vec2 uv = vec2(uvs_interpolated[0][0], uvs_interpolated[0][1]);
    vec2 uv_dx = vec2(uvs_interpolated[1][0], uvs_interpolated[1][1]);
    vec2 uv_dy = vec2(uvs_interpolated[2][0], uvs_interpolated[2][1]);

    Material material = get_material(visibility_info);
    if (material.albedo_texture_index != -1)
        out_color = textureGrad(nonuniformEXT(sampler2D(u_textures[nonuniformEXT(material.albedo_texture_index)], u_sampler)), uv, uv_dx, uv_dy);
    else
        out_color = material.albedo_color;
}