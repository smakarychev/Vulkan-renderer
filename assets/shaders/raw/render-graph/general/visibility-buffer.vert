#version 460

#include "common.glsl"

#extension GL_ARB_shader_draw_parameters: enable

layout(constant_id = 0) const bool COMPOUND_INDEX = true;

layout(set = 1, binding = 0) uniform camera_buffer {
    CameraGPU camera;
} u_camera;

layout(std430, set = 1, binding = 1) readonly buffer position_buffer {
    Position positions[];
} u_positions;

layout(std430, set = 1, binding = 2) readonly buffer uv_buffer {
    UV uvs[];
} u_uv;

layout(std430, set = 1, binding = 3) readonly buffer object_buffer {
    object_data objects[];
} u_objects;

layout(std430, set = 1, binding = 4) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

layout(push_constant) uniform push_constants {
    uint u_command_offset;
};

layout(location = 0) out uint vertex_command_id;
layout(location = 1) out vec2 vertex_uv;

void main() {
    uint argument_index;
    IndirectCommand command;
    
    if (COMPOUND_INDEX) {
        const VisibilityInfo visibility_info = unpack_visibility(gl_VertexIndex);
        const uint command_id = visibility_info.instance_id;
        vertex_command_id = command_id;
        const uint index = visibility_info.triangle_id;

        command = u_commands.commands[command_id];

        argument_index = command.vertexOffset + index;
    }
    else {
        vertex_command_id = gl_DrawIDARB + u_command_offset;
        command = u_commands.commands[vertex_command_id];
        
        argument_index = gl_VertexIndex;
    }

    const uint object_index = command.render_object;
    const mat4 model = u_objects.objects[object_index].model;

    const Position position = u_positions.positions[argument_index];
    const vec3 position_v = vec3(position.x, position.y, position.z);

    const UV uv = u_uv.uvs[argument_index];
    vertex_uv = vec2(uv.u, uv.v);
    
    gl_Position = u_camera.camera.view_projection * model * vec4(position_v, 1.0);
}