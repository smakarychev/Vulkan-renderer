#version 460

layout(location = 0) in vec3 vert_color;

layout(location = 0) out vec4 out_color;

void main() {

    vec2 coord = gl_PointCoord - vec2(0.5);
    out_color = vec4(vert_color, 0.5 - length(coord));
    if (out_color.a < 0.1)
        discard;
}