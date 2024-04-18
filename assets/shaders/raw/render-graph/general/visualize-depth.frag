#version 460

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vertex_uv;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_depth;

void main() {
    const float val = textureLod(sampler2D(u_depth, u_sampler), vertex_uv, 0).r;
    out_color = vec4(vec3(val), 1.0f);
}