#version 450
#pragma shader_stage(vertex)

layout(binding = 0) uniform u_Transform {
    mat4 model;
    mat4 view;
    mat4 proj;
} u_transform;

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec3 a_color;

layout(location = 0) out vec3 frag_color;

void main() {
    gl_Position = u_transform.proj * u_transform.view * u_transform.model * vec4(a_position, 0.0, 1.0);
    frag_color = a_color;
}