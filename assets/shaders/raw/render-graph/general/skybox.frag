#version 460

#include "../pbr/common.glsl"

layout(location = 0) in vec3 cube_uv;

layout(location = 0) out vec4 out_color;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform textureCube u_skybox;

layout(set = 1, binding = 1) uniform view_info {
    ViewInfo view;
} u_view_info;

layout(push_constant) uniform push_constants {
    float u_lod_bias;
};

void main() {
    vec3 color = textureLod(samplerCube(u_skybox, u_sampler), cube_uv, u_lod_bias).rgb;
    color = tonemap(color, u_view_info.view.environment_power);
    out_color = vec4(color, 1.0f);
}