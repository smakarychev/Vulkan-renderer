#version 460

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

layout(push_constant) uniform constants {
    vec4 data;
    mat4 transform;
} u_constants;

@dynamic
layout(set = 0, binding = 0) uniform camera_buffer {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
} u_camera_buffer;

struct object_data{
    mat4 model;
};

layout(std140,set = 1, binding = 0) readonly buffer object_buffer{
    object_data objects[];
} u_object_buffer;


layout(location = 0) out vec3 vert_normal;
layout(location = 1) out vec2 vert_uv;
layout(location = 2) out int vert_instance_id;

void main() {
    gl_Position = u_camera_buffer.view_projection * u_object_buffer.objects[gl_BaseInstance].model * vec4(a_position, 1.0);
    vert_normal = a_normal;
    vert_uv = a_uv;
    vert_instance_id = gl_BaseInstance;
}