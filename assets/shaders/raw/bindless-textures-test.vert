#version 460

#extension GL_ARB_shader_draw_parameters : enable

@binding : 0
layout(location = 0) in vec3 a_position;
@binding : 1
layout(location = 1) in vec3 a_normal;
@binding : 2
layout(location = 2) in vec2 a_uv;

struct IndirectCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;

    uint render_object;
};

const uint TRIANGLES_PER_MESHLET = 256;

@dynamic
layout(set = 0, binding = 0) uniform camera_buffer {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
} u_camera_buffer;

struct object_data{
    mat4 model;
    // bounding sphere
    float x;
    float y;
    float z;
    float r;
};

layout(std430, set = 1, binding = 0) readonly buffer object_buffer {
    object_data objects[];
} u_object_buffer;

layout(std430, set = 2, binding = 2) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_command_buffer;

layout(location = 0) out vec3 vert_normal;
layout(location = 1) out vec2 vert_uv;
layout(location = 2) out uint vert_command_id;  
layout(location = 3) out uint vert_triangle_offset;

void main() {
    IndirectCommand command = u_command_buffer.commands[gl_DrawIDARB];
    uint object_index = command.render_object;
    gl_Position = u_camera_buffer.view_projection * u_object_buffer.objects[object_index].model * vec4(a_position, 1.0);
    vert_normal = a_normal;
    vert_uv = a_uv;

    vert_command_id = gl_DrawIDARB;
    vert_triangle_offset = command.firstIndex / 3;
}