#version 460

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vertex_uv;

layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_brdf;

void main() {
    const vec2 brdf = textureLod(sampler2D(u_brdf, u_sampler), vertex_uv, 0).rg;
    out_color = vec4(brdf, 0.0f, 1.0f);
}