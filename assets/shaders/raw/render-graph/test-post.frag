#version 460

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vert_uv;

layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_image;
layout(set = 1, binding = 1) uniform time {
    float time;
} u_time;

vec2 uv_transform(vec2 uv) {
    
    uv -= vec2(0.5f);
    uv = uv * 1.2f * (1.0f / 1.2f + 2.0f * uv.x * uv.x * uv.y * uv.y);
    uv += vec2(0.5f);
        
    return uv;
}

vec3 scanline(vec3 color, vec2 uv) {
    float scanline = clamp(0.95f + 0.05f * cos(3.14f * (-uv.y + 0.008f * u_time.time) * 240.0f * 1.0f), 0.0f, 1.0f);
    float grille = 0.85f + 0.15f * clamp(1.5f * cos(3.14f * uv.x * 640.0f * 1.0f), 0.0f, 1.0f);
    color *= scanline * grille * 1.2f;
    
    return color;
}

vec3 vignette(vec3 color, vec2 uv) {
    float vignette = uv.x * uv.y * (1.0f - uv.x) * (1.0f - uv.y);
    vignette = clamp(pow(16.0f * vignette, 0.3f), 0.0f, 1.0f);
    color *= vignette;
    
    return color;
}

vec3 split_colors(vec2 uv) {
    vec3 color;
    color.r = texture(sampler2D(u_image, u_sampler), vert_uv + vec2(0.0f, 0.01f)).r;
    color.g = texture(sampler2D(u_image, u_sampler), vert_uv).g;
    color.b = texture(sampler2D(u_image, u_sampler), vert_uv - vec2(0.0f, 0.01f)).b;
    
    return color;
}

void main() {
    vec2 uv = uv_transform(vert_uv);
    vec3 color = split_colors(vert_uv);
    color = scanline(color, vert_uv);
    color = vignette(color, vert_uv);
    out_color = vec4(color, 1.0f);
}