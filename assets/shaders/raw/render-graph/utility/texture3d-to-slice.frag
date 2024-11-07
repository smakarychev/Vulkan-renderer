#version 460

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vertex_uv;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture3D u_texture;

layout(push_constant) uniform push_constants {
    float u_slice_normalized;
};

void main() {
    const vec3 color = textureLod(sampler3D(u_texture, u_sampler), vec3(vertex_uv, u_slice_normalized), 0).rgb;
    out_color = vec4(color, 1.0f);
}