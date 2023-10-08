#version 460

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec4 a_color;

layout(location = 0) out vec3 vert_color;

void main() {

    gl_PointSize = 14.0;
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
    vert_color = a_color.rgb;
}