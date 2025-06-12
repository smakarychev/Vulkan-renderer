#version 460

#include "../../view_info.glsl"

layout(location = 0) out vec3 cube_uv;

const vec2 vertices[6] = {
    vec2(-1.0f, -1.0f),
    vec2(-1.0f,  1.0f),
    vec2( 1.0f,  1.0f),

    vec2(-1.0f, -1.0f),
    vec2( 1.0f,  1.0f),
    vec2( 1.0f, -1.0f),
};

layout(set = 1, binding = 1) uniform view_info {
    ViewInfo view;
} u_view_info;

layout(push_constant) uniform push_constants {
    float u_lod_bias;
};

void main() {
    const vec4 position = vec4(vertices[gl_VertexIndex], 0.0f, 1.0f);
    const vec3 unprojected = vec3(u_view_info.view.inv_projection * position);
    cube_uv = mat3(u_view_info.view.inv_view) * unprojected;
    
    gl_Position = position;
}