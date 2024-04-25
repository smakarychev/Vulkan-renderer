#version 460

#include "../../shadow.glsl"

#extension GL_EXT_scalar_block_layout: require

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vertex_uv;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2DArray u_shadow_map;

layout(scalar, set = 1, binding = 1) uniform csm_data {
    CSMData csm_data;
} u_csm_data;

layout(push_constant) uniform push_constants {
    uint u_cascade_index;
};

void main() {
    const float val = textureLod(sampler2DArray(u_shadow_map, u_sampler), vec3(vertex_uv, float(u_cascade_index)), 0).r;
    out_color = vec4(vec3(val), 1.0f);
}