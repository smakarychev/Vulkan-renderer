#version 460

@binding : 0
layout(location = 0) in vec3 a_position;
@binding : 1
layout(location = 1) in vec3 a_normal;
@binding : 2
layout(location = 2) in vec3 a_tangent;
@binding : 3
layout(location = 3) in vec2 a_uv;

layout(set = 0, binding = 0) uniform camera {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
} u_camera;

struct VertexOut {
    vec3 normal;
};

layout(location = 0) out VertexOut vertex_out;

void main() {
    gl_Position = u_camera.view_projection * vec4(a_position, 1.0);
    vertex_out.normal = a_normal;
}