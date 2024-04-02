#version 460

#include "../common.glsl"
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
    mat4 view_projection;
} u_camera;

layout(std430, set = 1, binding = 1) readonly buffer object_buffer {
    object_data objects[];
} u_objects;

layout(std430, set = 1, binding = 2) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

struct VertexOut {
    vec3 normal;
};

layout(location = 0) out VertexOut vertex_out;

void main() {
    IndirectCommand command = u_commands.commands[gl_DrawIDARB];
    uint object_index = command.render_object;
    gl_Position = u_camera.view_projection * u_objects.objects[object_index].model * vec4(a_position, 1.0);
    vertex_out.normal = a_normal;
}