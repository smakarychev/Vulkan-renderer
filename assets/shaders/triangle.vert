#version 460
#pragma shader_stage(vertex)

layout(binding = 0) uniform u_Transform {
    mat4 model;
    mat4 view;
    mat4 proj;
} u_transform;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;
layout(location = 2) in vec2 a_uv;

layout(location = 0) out vec3 frag_color;
layout(location = 1) out vec2 frag_uv;

void main() {
    gl_Position = u_transform.proj * u_transform.view * u_transform.model * vec4(a_position, 1.0);
    frag_color = a_color;
    frag_uv = a_uv;
}