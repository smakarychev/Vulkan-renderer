#version 460

#extension GL_EXT_samplerless_texture_functions: enable

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vertex_uv;

layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_image;
layout(set = 1, binding = 1) uniform time {
    float time;
} u_time;

layout(set = 1, binding = 2) uniform settings {
    float curvature;
    float color_split;
    float lines_multiplier;
    float vignette_power;
    float vignette_radius;
} u_settings;

vec2 uv_transform(vec2 uv, vec2 curvature) {
    uv = uv * 2.0f - 1.0f;
    vec2 offset = abs(uv.yx) * vec2(curvature.x, curvature.y);
    uv = uv + uv * offset * offset;
    uv = uv * 0.5f + 0.5f;

    return uv;
}

vec3 scanline(vec3 color, vec2 uv) {
    float scanline = clamp(0.95f + 0.05f * cos(3.14f * (-uv.y + 0.008f * u_time.time) * 240.0f * 1.0f), 0.0f, 1.0f);
    float grille = 0.85f + 0.15f * clamp(1.5f * cos(3.14f * uv.x * 640.0f * 1.0f), 0.0f, 1.0f);
    color *= scanline * grille * 1.2f;

    return color;
}

vec3 vignette(vec3 color, vec2 uv) {
    uv *= 1.0f - uv.yx; 
    float vignette = uv.x * uv.y * u_settings.vignette_radius * textureSize(u_image, 0).x;
    vignette = clamp(pow(vignette, u_settings.vignette_power), 0.0f, 1.0f);
    color *= vignette;

    return color;
}

vec3 split_colors(vec2 uv, float split) {
    vec2 uvr = uv + vec2(0.0f, split);
    if (uvr.y > 1.0f) uvr = uv;
    vec2 uvb = uv - vec2(0.0f, split);
    if (uvb.y < 0.0f) uvb = uv;
    
    vec3 color;
    color.r = textureLod(sampler2D(u_image, u_sampler), uvr, 0).r;
    color.g = textureLod(sampler2D(u_image, u_sampler), uv, 0).g;
    color.b = textureLod(sampler2D(u_image, u_sampler), uvb, 0).b;

    return color;
}

void main() {
    vec2 uv = uv_transform(vertex_uv, vec2(u_settings.curvature));
    if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1) {
        out_color = vec4(vec3(0.0f), 1.0f);
        return;
    }
    vec3 color = split_colors(uv, u_settings.color_split);
    color = vignette(color, uv);
    //// loophero
    color.g *= (sin(uv.y * textureSize(u_image, 0).y * u_settings.lines_multiplier) + 1.0f) * 0.15f + 1.0f;
    color.rb *= (cos(uv.y * textureSize(u_image, 0).y * u_settings.lines_multiplier) + 1.0f) * 0.135f + 1.0f;
    out_color = vec4(color, 1.0f);
}