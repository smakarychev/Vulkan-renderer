#version 460
#pragma shader_stage(fragment)

layout(location = 0) in vec3 frag_color;
layout(location = 1) in vec2 frag_uv;

layout(location = 0) out vec4 out_color;

layout(binding = 1) uniform sampler2D u_sampler;

void main() {
    out_color = texture(u_sampler, frag_uv);
}