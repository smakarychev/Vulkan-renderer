#version 460

#include "../../camera.glsl"
#include "../tonemapping.glsl"

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vertex_uv;

@immutable_sampler_clamp_edge
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_transmittance_lut;

layout(set = 1, binding = 1) uniform camera_buffer {
    CameraGPU camera;
} u_camera;

layout(push_constant) uniform push_constants {
    float u_frame_tick;
};

const float PI = 3.14159265358;

const vec3 center = vec3(0, 0, -7);
const float no_hit = 3.402823466e+38f;

vec3 light_dir = normalize(vec3(
    0,  
    1 - sin(u_frame_tick / 100),
    //1, 
    1));




// todo: remove everything above me
const float surface_radius = 6.360f;
const float atmosphere_radius = 6.460f;

const vec3  rayleigh_scattering_base = vec3(5.802f, 13.558f, 33.1f);
const float rayleigh_absorption_base = 0.0f;
const vec3  mie_scattering_base = vec3(3.996f);
const float mie_absorption_base = 4.4f;
const vec3  ozone_absorption_base = vec3(0.650f, 1.881f, 0.085f);

const float transmittance_steps = 40.0f;
const float sky_steps = 12.0f;

// todo: remove me
#extension GL_EXT_debug_printf : enable
float distance_to_atmosphere_top(float r, float mu) {
    if (r < surface_radius || r > atmosphere_radius || mu < -1 || mu > 1)
        debugPrintfEXT("%f %f\n", r, mu);
    const float discriminant = r * r * (mu * mu - 1.0f) + atmosphere_radius * atmosphere_radius;

    return max(0.0f, -r * mu + sqrt(max(0.0, discriminant)));
}


vec2 transmittance_uv_from_r_mu(float r, float mu) {
    const float H = sqrt(atmosphere_radius * atmosphere_radius - surface_radius * surface_radius);
    const float rho = sqrt(max(r * r - surface_radius * surface_radius, 0.0f));

    const float d = distance_to_atmosphere_top(r, mu);
    const float d_min = atmosphere_radius - r;
    const float d_max = H + rho;

    const float x_mu = (d - d_min) / (d_max - d_min);
    const float x_r = rho / H;
    if (d > d_max) {
        debugPrintfEXT("%f %f %f %f %f %f\n", d, d_max, r, mu, H, rho);
    }
    return vec2(x_mu, x_r);
}

struct Intersection {
    float t;
    float depth;
};

Intersection intersect_sphere(vec3 ro, vec3 rd, vec3 center, float radius) {
    const vec3 oc = ro - center;
    const float b = dot(oc, rd);
    const float c = dot(oc, oc) - radius * radius;
    const float t = b * b - c;
    // no hit
    if (t < 0.0f)
        return Intersection(no_hit, no_hit);
    
    const float tsqrt = sqrt(t);
    if (-b < -tsqrt)
        return Intersection(no_hit, no_hit);

    if (-b > tsqrt)
        return Intersection(-b - tsqrt, 2.0f * tsqrt);
    return Intersection(0.0f, -b + tsqrt);
}

struct ScatteringCoefficientsData {
    vec3 rayleigh, mie, extinction;  
};

float rayleigh_phase(float cos_theta) {
    return 3.0f / (16.0f * PI) * (1.0f + cos_theta * cos_theta);
}

float mie_phase(float cos_theta) {
    const float g = 0.8f;
    const float scale = 3.0f / (8.0f * PI);
    const float nom = scale * (1.0f - g * g) * (1.0f + cos_theta * cos_theta);
    const float denom = (2.0f + g * g) * pow(1.0f + g * g - 2.0f * g * cos_theta, 1.5f);
    
    return nom / denom; 
}

ScatteringCoefficientsData calculate_scattering(vec3 x, vec3 center) {
    const float altitude_km = (length(x - center) - surface_radius) * 1000;
    const float rayleigh_density = exp(-altitude_km / 8.0f);
    const float mie_density = exp(-altitude_km / 1.2f);

    ScatteringCoefficientsData scattering;
    scattering.rayleigh = rayleigh_scattering_base * rayleigh_density;
    scattering.mie = mie_scattering_base * mie_density;
    const float rayleigh_absorption = rayleigh_absorption_base * rayleigh_density;
    const float mie_absorption = mie_absorption_base * mie_density;
    
    const vec3 ozone_absorption = ozone_absorption_base * max(0.0f, 1.0f - abs(altitude_km - 25.0f) / 15.0f);
    
    scattering.extinction =
        scattering.rayleigh + rayleigh_absorption +
        scattering.mie + mie_absorption +
        ozone_absorption;
    return scattering;
}

float calculate_visibility(vec3 ro, vec3 rd) {
    const float surface = intersect_sphere(ro, rd, center, surface_radius).t;
    const Intersection atmosphere = intersect_sphere(ro, rd, center, atmosphere_radius);

    return (surface == no_hit || atmosphere.t + atmosphere.depth < surface) ? 1.0f : 0.0f;
}

vec3 calculate_transmittance(vec3 ro, vec3 rd, float len, vec3 center) {
    // e^(-integral(extinction(x) * dx))
    vec3 total_extinction = vec3(0.0f);
    const float step_size = len / transmittance_steps;
    for (float i = 0.0f; i < transmittance_steps; i += 1.0f) {
        const vec3 x = ro + rd * step_size * i;
        const vec3 extinction = calculate_scattering(x, center).extinction;
        total_extinction += extinction;
    }
    
    return exp(-total_extinction * step_size);
}

vec3 calculate_s(vec3 x, vec3 light_dir, vec3 center) {
    const vec3 rel = x - center;
    const float r = min(length(rel), atmosphere_radius);
    const vec3 up = rel / r;
    const float mu = dot(light_dir, up);
    const vec2 uv = transmittance_uv_from_r_mu(r, mu);
    return calculate_visibility(x, light_dir) * textureLod(sampler2D(u_transmittance_lut, u_sampler), uv, 0).rgb; 
}

vec3 calculate_in_scattering(vec3 ro, vec3 rd, vec3 x, vec3 light_dir) {
    const ScatteringCoefficientsData scattering_data = calculate_scattering(x, center);
    const vec3 scattering = 
        scattering_data.rayleigh * rayleigh_phase(dot(rd, light_dir)) +
        scattering_data.mie * mie_phase(dot(rd, light_dir));
    return 
        calculate_transmittance(ro, rd, length(x - ro), center) *
        scattering * 
        calculate_s(x, light_dir, center) * 40;
}

vec3 sky_color(vec3 ro, vec3 rd, vec3 light_dir, float depth) {
    vec3 scattering = vec3(0.0f);
    
    for (float i = 0.0; i < sky_steps; i += 1.0f) {
        const float dt = depth / sky_steps;
        const vec3 x = ro + rd * dt * i;
        scattering += calculate_in_scattering(ro, rd, x, light_dir) * dt;
    }
    
    return scattering;
}

void generate_transmittance_lut() {
    const float sun_cos_theta = vertex_uv.x * 2.0f - 1.0f;
    const float sun_sin_theta = sqrt(1.0f - clamp(sun_cos_theta * sun_cos_theta, 0.0f, 1.0f));
    const float view_height = mix(surface_radius, atmosphere_radius, 1.0f - vertex_uv.y);
    
    const vec3 sun_dir = normalize(vec3(0.0f, sun_cos_theta, sun_sin_theta));
    const vec3 view_pos = vec3(0.0f, view_height, 0.0f);
    const vec3 center = vec3(0.0f);
    
    if (intersect_sphere(view_pos, sun_dir, vec3(0.0f), surface_radius).t != no_hit) {
        out_color = vec4(vec3(0.0f), 1.0f);
        return;
    }
    
    const Intersection to_sun = intersect_sphere(view_pos, sun_dir, center, atmosphere_radius);
    const vec3 transmittance = calculate_transmittance(view_pos, sun_dir, to_sun.depth, center);
    
    out_color = vec4(transmittance, 1.0f);
}

float sky_view_zenith(float v, float view_height) {
    const float tangent = sqrt(view_height * view_height - surface_radius * surface_radius);
    const float cos_theta = clamp(tangent / view_height, 0.0f, 1.0f);
    const float theta = acos(-cos_theta);
    const float true_horizon = PI - theta;
    if (v < 0.5f) {
        v = 1.0f - v * 2.0f;
        v = 1.0f - v * v;
        return true_horizon * v; // from 0 to true_horizon
    } else {
        v = 1.0f - v * 2.0f;
        v = v * v;
        return true_horizon + theta * v; // from true_horizon to pi
    }
}

void generate_sky_view_lut() {
    const vec3 position = center + vec3(0, surface_radius + 0.00012f, 0);
    const float view_height = length(position - center);
    
    const float cos_latitude = cos((vertex_uv.x - 0.5f) * 2.0 * PI);
    const float sin_latitude = sin((vertex_uv.x - 0.5f) * 2.0 * PI);
    const float cos_zenith = cos(sky_view_zenith(vertex_uv.y, view_height));
    const float sin_zenith = sqrt(1.0f - cos_zenith * cos_zenith);
    
    // todo: change me to ordinary spherical coorditanes
    const vec3 view_direction_local = vec3(
        sin_zenith * sin_latitude,
        cos_zenith,
        -sin_zenith * cos_latitude);
    
    const vec3 view_direction = view_direction_local;
    
    const float sun_height = 0.3;
    const float cos_sun_zenith = cos(sun_height);
    const float sin_sun_zenith = sin(sun_height);
    //const vec3 up = position / view_height;
    //const float cos_sun_zenith = dot(light_dir, up);
    //const float sin_sun_zenith = sqrt(1.0f - cos_sun_zenith * cos_sun_zenith);
    const vec3 sun_direction = normalize(vec3(0.0f, sin_sun_zenith, -cos_sun_zenith));

    const Intersection atmosphere_intersection = intersect_sphere(position, view_direction, center, atmosphere_radius);
    const Intersection surface_intersection = intersect_sphere(position, view_direction, center, surface_radius);
    const float depth = min(
        atmosphere_intersection.depth,
        surface_intersection.t - atmosphere_intersection.t);
    
    const vec3 color = sky_color(position + view_direction * atmosphere_intersection.t, view_direction, sun_direction, depth);
    
    out_color = vec4(color, 1.0f);
}


void main() {
    //generate_transmittance_lut();
    //generate_sky_view_lut();
    //return;
    
    const vec2 aspect = vec2(u_camera.camera.resolution.x / u_camera.camera.resolution.y, 1.0f);
    const vec3 local_ray_dir = vec3(aspect * (vec2(vertex_uv.x, 1.0f - vertex_uv.y) - 0.5f) * 2.0f, -1.0f);
    const vec3 ro = u_camera.camera.position;
    const vec3 rd = normalize(vec3(u_camera.camera.inv_view * vec4(local_ray_dir, 0.0f)));
    
    const Intersection atmosphere_intersection = intersect_sphere(ro, rd, center, atmosphere_radius); 
    if (atmosphere_intersection.depth == no_hit) {
        out_color = vec4(vec3(0.0f), 1.0f);

        return;
    }
    
    // in the actual scene, it will be a read from depth buffer
    const Intersection surface_intersection = intersect_sphere(ro, rd, center, surface_radius);
    const float depth = min(
        atmosphere_intersection.depth,
        surface_intersection.t - atmosphere_intersection.t);
    
    const vec3 transmittance = calculate_transmittance(ro + rd * atmosphere_intersection.t, rd, depth * 0.9, center);
    
    out_color = vec4(vec3(calculate_visibility(ro, rd)), 1.0f);
    //out_color = vec4(transmittance, 1.0f);
    if (atmosphere_intersection.t  < 0) {
        out_color = vec4(1.0f);
    }
    
    vec3 color = sky_color(ro + rd * atmosphere_intersection.t, rd, light_dir, depth);
    color = tonemap(color, 2.0f);
    out_color = vec4(color, 1.0f);
    //out_color = vec4(transmittance, 1.0f);
}