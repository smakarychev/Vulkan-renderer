#version 460

layout (location = 0) out vec2 vert_uv;
layout (location = 1) out vec2 vert_position;

void main()
{
    vert_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vert_position = vert_uv * 2.0f + -1.0f;
    gl_Position = vec4(vert_position, 0.0f, 1.0f);
}