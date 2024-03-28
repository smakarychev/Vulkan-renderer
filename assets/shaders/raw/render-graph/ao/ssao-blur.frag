#version 460

layout(location = 0) out float out_color;

layout(location = 0) in vec2 vertex_uv;

layout(constant_id = 0) const bool IS_VERTICAL = false;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_ssao;

void main() {
    float ssao = textureLod(sampler2D(u_ssao, u_sampler), vertex_uv, 0).r;
    out_color = ssao;
}