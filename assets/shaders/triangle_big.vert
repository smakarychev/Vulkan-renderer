#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_color;

layout(push_constant) uniform constants {
    vec4 data;
    mat4 transform;
} u_constants;

layout(location = 0) out vec3 frag_color;

void main() {
    gl_Position = u_constants.transform * vec4(a_position, 1.0);
    frag_color = a_color;
}