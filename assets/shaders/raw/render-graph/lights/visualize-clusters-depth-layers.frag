#version 460

#include "common.glsl"

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vertex_uv;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_depth;

layout(push_constant) uniform push_constants {
    float u_near;
    float u_far;
};

uint hash(uint x) {
    uint state = x * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    
    return (word >> 22u) ^ word;
}

vec3 color_hash(uint x) {
    const uint hash_val = hash(x);
    
    return vec3(
        float(hash_val & 255u) / 255.0f,
        float((hash_val >> 8) & 255u) / 255.0f,
        float((hash_val >> 16) & 255u) / 255.0f); 
}

void main() {
    const float depth = textureLod(sampler2D(u_depth, u_sampler), vertex_uv, 0).r;
    const uint slice = slice_index(depth, u_near, u_far, LIGHT_CLUSTER_BINS_Z);
    out_color = vec4(color_hash(slice), 1.0f);
}