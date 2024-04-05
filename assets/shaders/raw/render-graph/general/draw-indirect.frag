#version 460

layout(location = 0) out vec4 out_color;

struct VertexOut {
    vec3 normal;
};

layout(location = 0) in VertexOut vertex_out;

void main() {
    out_color = vec4((vertex_out.normal + 1.0f) / 2.0f, 1.0f);
}