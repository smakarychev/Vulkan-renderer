#version 460

layout(location = 0) out vec3 cube_uv;

const vec2 vertices[6] = {
    vec2(-1.0f, -1.0f),
    vec2( 1.0f,  1.0f),
    vec2(-1.0f,  1.0f),

    vec2(-1.0f, -1.0f),
    vec2( 1.0f, -1.0f),
    vec2( 1.0f,  1.0f),
};

layout(set = 1, binding = 1) uniform projection {
    mat4 projection_inverse;
    mat4 view_inverse;
} u_projection;

layout(push_constant) uniform push_constants {
    float u_lod_bias;
};

void main() {
    const vec4 position = vec4(vertices[gl_VertexIndex], 0.0f, 1.0f);
    const vec3 unprojected = vec3(u_projection.projection_inverse * position);
    cube_uv = mat3(u_projection.view_inverse) * unprojected;
    
    gl_Position = position;
}