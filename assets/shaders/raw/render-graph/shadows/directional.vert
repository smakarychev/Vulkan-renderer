#version 460

#include "../common.glsl"
#include "../../camera.glsl"

#extension GL_ARB_shader_draw_parameters: enable

layout(constant_id = 0) const bool COMPOUND_INDEX = false;

layout(set = 1, binding = 0) uniform camera {
    CameraGPU camera;
} u_camera;

layout(std430, set = 1, binding = 1) readonly buffer positions_buffer {
    Position positions[];
} u_positions;

layout(std430, set = 1, binding = 2) readonly buffer object_buffer {
    object_data objects[];
} u_objects;

layout(std430, set = 1, binding = 3) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

layout(push_constant) uniform push_constants {
    uint u_command_offset;
};

void main() {
    uint argument_index;
    IndirectCommand command;

    if (COMPOUND_INDEX) {
        const UnpackedIndex unpacked_index = unpack_index(gl_VertexIndex);
        const uint command_id = unpacked_index.instance;
        const uint index = unpacked_index.index;

        command = u_commands.commands[command_id];

        argument_index = command.vertexOffset + index;
    }
    else {
        command = u_commands.commands[gl_DrawIDARB + u_command_offset];

        argument_index = gl_VertexIndex;
    }

    const uint object_index = command.render_object;
    const mat4 model = u_objects.objects[object_index].model;

    const Position position = u_positions.positions[argument_index];
    gl_Position = u_camera.camera.view_projection * model * vec4(position.x, position.y, position.z, 1.0f);
}