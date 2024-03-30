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


layout(push_constant) uniform push_constants {
    mat4 u_projection_inverse;
    mat4 u_view_inverse;
};

void main() {
    const vec4 position = vec4(vertices[gl_VertexIndex], 0.0f, 1.0f);
    const vec3 unprojected = vec3(u_projection_inverse * position);
    cube_uv = mat3(u_view_inverse) * unprojected;
    
    gl_Position = position;
}