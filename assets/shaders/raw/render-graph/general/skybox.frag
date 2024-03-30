#version 460

layout(location = 0) in vec3 cube_uv;

layout(location = 0) out vec4 out_color;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform textureCube u_skybox;

void main() {
    const vec4 color = textureLod(samplerCube(u_skybox, u_sampler), cube_uv, 0);
    out_color = vec4(color);
}