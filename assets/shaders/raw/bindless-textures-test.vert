#version 460

@binding : 0
layout(location = 0) in vec3 a_position;
@binding : 1
layout(location = 1) in vec3 a_normal;
@binding : 2
layout(location = 2) in vec2 a_uv;

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

layout(std430, set = 1, binding = 0) readonly buffer object_buffer{
    object_data objects[];
} u_object_buffer;

struct Meshlet
{
    uint bounding_cone;
    // bounding sphere
    float x;
    float y;
    float z;
    float r;
    uint render_object;
    // these 3 values actually serve as a padding, but might also be used as a debug meshlet shading
    float R;
    float G;
    float B;
    uint pad0;
    uint pad1;
    uint pad2;
};

layout(std430, set = 2, binding = 2) readonly buffer meshlet_buffer {
    Meshlet meshlets[];
} u_meshlet_buffer;

layout(location = 0) out vec3 vert_normal;
layout(location = 1) out vec2 vert_uv;
layout(location = 2) out int vert_instance_id;

void main() {
    uint object_index = u_meshlet_buffer.meshlets[gl_BaseInstance].render_object;
    gl_Position = u_camera_buffer.view_projection * u_object_buffer.objects[object_index].model * vec4(a_position, 1.0);
    vert_normal = a_normal;
    vert_uv = a_uv;
    vert_instance_id = gl_BaseInstance;
}