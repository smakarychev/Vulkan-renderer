#version 460

#include "common.glsl"

#extension GL_ARB_shader_draw_parameters: enable

@binding : 0
layout(location = 0) in vec3 a_position;
@binding : 1
layout(location = 1) in vec3 a_normal;
@binding : 2
layout(location = 2) in vec3 a_tangent;
@binding : 3
layout(location = 3) in vec2 a_uv;

layout(set = 1, binding = 0) uniform camera {
    CameraGPU camera;
} u_camera;

layout(std430, set = 1, binding = 1) readonly buffer object_buffer {
    object_data objects[];
} u_objects;

layout(std430, set = 1, binding = 2) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

layout(location = 0) out uint vertex_object_index;
layout(location = 1) out vec3 vertex_position;
layout(location = 2) out vec3 vertex_normal;
layout(location = 3) out vec3 vertex_tangent;
layout(location = 4) out vec2 vertex_uv;

void main() {
    IndirectCommand command = u_commands.commands[gl_DrawIDARB];
    vertex_object_index = command.render_object;

    const mat4 model = u_objects.objects[vertex_object_index].model;

    vec4 v_position = u_camera.camera.view_projection * model * vec4(a_position, 1.0f);
    vertex_position = v_position.xyz;

    vertex_normal = transpose(inverse(mat3(model))) * vec3(a_normal);
    vertex_tangent = mat3(model) * vec3(a_tangent);
    vertex_uv = a_uv;

    gl_Position = v_position;
}