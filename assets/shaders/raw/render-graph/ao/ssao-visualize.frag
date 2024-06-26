#version 460

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vertex_uv;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_ssao;

void main() {
    const float ssao = textureLod(sampler2D(u_ssao, u_sampler), vertex_uv, 0).r;
    out_color = vec4(vec3(ssao), 1.0f);
}