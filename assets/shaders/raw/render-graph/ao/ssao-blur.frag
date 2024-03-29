#version 460

#extension GL_EXT_samplerless_texture_functions: enable

layout(location = 0) out float out_color;

layout(location = 0) in vec2 vertex_uv;

layout(constant_id = 0) const bool IS_VERTICAL = false;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_ssao;

void main() {
    const int kernel_size = 2;
    ivec2 ssao_size = textureSize(u_ssao, 0);
    
    float ssao = 0.0f;
    
    for (int i = -kernel_size; i <= kernel_size; i++) {
        vec2 delta;
        if (IS_VERTICAL)
            delta = vec2(0.0f, float(i)) / ssao_size;
        else
            delta = vec2(float(i), 0.0f) / ssao_size;

        ssao += textureLod(sampler2D(u_ssao, u_sampler), vertex_uv + delta, 0).r;
    }
    
    ssao /= 2 * kernel_size + 1;
    
    out_color = ssao;
}