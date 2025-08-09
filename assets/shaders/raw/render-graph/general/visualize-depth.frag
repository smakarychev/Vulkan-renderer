#version 460

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vertex_uv;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_depth;

layout(push_constant) uniform push_constants {
    float u_near;
    float u_far;
    bool u_is_orthographic;
};

float linearize(float depth) {
    const float n = u_near;
    const float f = u_far;
    const float z = depth;

    if (u_is_orthographic)
        return z;
    
    return n / z;
}

void main() {
    const float val = textureLod(sampler2D(u_depth, u_sampler), vertex_uv, 0).r;
    out_color = vec4(vec3(linearize(val)), 1.0f);
}