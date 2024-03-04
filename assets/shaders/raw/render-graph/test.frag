#version 460

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform color {
    vec3 color;
} u_color;

void main() {
    out_color = vec4(u_color.color, 1.0f);
}