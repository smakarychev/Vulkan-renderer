// https://theorangeduck.com/page/pure-depth-ssao

#version 460

#extension GL_EXT_samplerless_texture_functions: enable

layout(location = 0) out float out_color;

layout(location = 0) in vec2 vertex_uv;

struct Settings {
    float total_strength;
    float base;
    float area;
    float falloff;
    float radius;
};

struct Camera {
    mat4 projection_inverse;
};

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_depth_sampler;
@immutable_sampler_nearest
layout(set = 0, binding = 1) uniform sampler u_noise_sampler;
layout(set = 1, binding = 0) uniform texture2D u_noise_texture;
layout(set = 1, binding = 1) uniform texture2D u_depth_texture;

layout(set = 1, binding = 2) uniform settings {
    Settings settings;
} u_settings;
layout(set = 1, binding = 3) uniform camera {
    Camera camera;
} u_camera;

vec3 normal_from_depth(float depth, ivec2 depth_size) {
    const vec2 offset_x = vec2(1.0f, 0.0f) / vec2(depth_size);
    const vec2 offset_y = vec2(0.0f, 1.0f) / vec2(depth_size);

    float depth1 = textureLod(sampler2D(u_depth_texture, u_depth_sampler), vertex_uv + offset_y, 0).r;
    float depth2 = textureLod(sampler2D(u_depth_texture, u_depth_sampler), vertex_uv + offset_x, 0).r;

    vec3 p1 = vec3(offset_y, depth - depth1);
    vec3 p2 = vec3(offset_x, depth - depth2);

    vec3 normal = cross(p1, p2);
    normal.z = -normal.z;

    return normalize(normal);
}

vec3 view_position_from_depth(float depth) {
    float x = vertex_uv.x * 2.0 - 1.0;
    float y = (1.0 - vertex_uv.y) * 2.0 - 1.0;
    vec4 pos = vec4(x, y, depth, 1.0);
    vec4 pos_view = u_camera.camera.projection_inverse * pos;
    return pos_view.xyz / pos_view.w;
}

void main() {
    const float total_strength = u_settings.settings.total_strength;
    const float base = u_settings.settings.base;
    const float area = u_settings.settings.area;
    const float falloff = u_settings.settings.falloff;
    const float radius = u_settings.settings.radius;

    const int samples = 16;
    vec3 sample_sphere[16] = {
        vec3( 0.5381f, 0.1856f,-0.4319f), vec3( 0.1379f, 0.2486f, 0.4430f),
        vec3( 0.3371f, 0.5679f,-0.0057f), vec3(-0.6999f,-0.0451f,-0.0019f),
        vec3( 0.0689f,-0.1598f,-0.8547f), vec3( 0.0560f, 0.0069f,-0.1843f),
        vec3(-0.0146f, 0.1402f, 0.0762f), vec3( 0.0100f,-0.1924f,-0.0344f),
        vec3(-0.3577f,-0.5301f,-0.4358f), vec3(-0.3169f, 0.1063f, 0.0158f),
        vec3( 0.0103f,-0.5869f, 0.0046f), vec3(-0.0897f,-0.4940f, 0.3287f),
        vec3( 0.7119f,-0.0154f,-0.0918f), vec3(-0.0533f, 0.0596f,-0.5411f),
        vec3( 0.0352f,-0.0631f, 0.5460f), vec3(-0.4776f, 0.2847f,-0.0271f)
    };
    
    ivec2 depth_size = textureSize(u_depth_texture, 0);
    ivec2 noise_size = textureSize(u_noise_texture, 0);
    vec2 noise_uv = vec2(float(depth_size.x) / float(noise_size.x),
                         float(depth_size.y) / float(noise_size.y))
                         * vertex_uv;
    vec3 noise = textureLod(sampler2D(u_noise_texture, u_noise_sampler), noise_uv, 0).rgb;
    float depth = textureLod(sampler2D(u_depth_texture, u_depth_sampler), vertex_uv, 0).r;

    vec3 position = vec3(vertex_uv, depth);
    vec3 normal = normal_from_depth(depth, depth_size);
    float radius_depth = radius / depth;
    float occlusion = 0.0f;
    for(int i = 0; i < samples; i++) {

        vec3 ray = radius_depth * reflect(sample_sphere[i], noise);
        vec3 hemi_ray = position + sign(dot(ray,normal)) * ray;

        float occ_depth = textureLod(sampler2D(u_depth_texture, u_depth_sampler), clamp(hemi_ray.xy, 0.0f, 1.0f), 0).r;
        float difference = occ_depth - depth;

        occlusion += step(falloff, difference) * (1.0f - smoothstep(falloff, area, difference));
    }

    float ao = 1.0 - total_strength * occlusion * (1.0f / samples);
    out_color = clamp(ao + base, 0.0f, 1.0f);
}