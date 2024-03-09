#version 460

#extension GL_EXT_samplerless_texture_functions: enable

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vert_uv;

layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_hiz;

layout(push_constant) uniform push_constants {
    uint mipmap;
    float intensity;
};

void main() {
    float depth = textureLod(sampler2D(u_hiz, u_sampler), vert_uv, float(mipmap)).r;
    out_color = vec4(vec3(depth) * intensity, 1.0f);
}