#version 460

layout(location = 0) out vec2 vertex_uv;
layout(location = 1) out vec2 vertex_position;

void main() {
    vertex_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vertex_position = vertex_uv * 2.0f + -1.0f;
    gl_Position = vec4(vertex_position, 0.0f, 1.0f);
}