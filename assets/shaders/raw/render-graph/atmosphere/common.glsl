#extension GL_EXT_scalar_block_layout: require

// todo: read these from cvars once I support it
#define TRANSMITTANCE_LUT_WIDTH 256
#define TRANSMITTANCE_LUT_HEIGHT 64

#define SKY_VIEW_LUT_WIDTH 200
#define SKY_VIEW_LUT_HEIGHT 100

#define PI 3.14159265359f

#define NO_HIT 3.402823466e+38f

#define TRANSMITTANCE_STEPS 40.0f
#define SKY_STEPS 30.0f

#include "../../light.glsl"

// needs scalar layout
struct AtmosphereSettings {
    float surface;
    float atmosphere;
    
    vec3 rayleigh_scattering;
    vec3 rayleigh_absorption;
    vec3 mie_scattering;
    vec3 mie_absorption;
    vec3 ozone_absorption;
    
    float rayleigh_density;
    float mie_density;
    float ozone_density;
};

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
        return Intersection(NO_HIT, NO_HIT);

    const float tsqrt = sqrt(t);
    if (-b < -tsqrt)
        return Intersection(NO_HIT, NO_HIT);

    if (-b > tsqrt)
        return Intersection(-b - tsqrt, 2.0f * tsqrt);

    return Intersection(0.0f, -b + tsqrt);
}

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

struct MediaSample {
    vec3 rayleigh, mie, extinction;
};


MediaSample sample_media(vec3 x, vec3 center, AtmosphereSettings atmosphere) {
    const float altitude_km = (length(x - center) - atmosphere.surface) * 1000;
    const float rayleigh_density = exp(-atmosphere.rayleigh_density * altitude_km / 8.0f);
    const float mie_density = exp(-atmosphere.mie_density * altitude_km / 1.2f);

    MediaSample media;
    media.rayleigh = atmosphere.rayleigh_scattering * rayleigh_density;
    media.mie = atmosphere.mie_scattering * mie_density;
    const vec3 rayleigh_absorption = atmosphere.rayleigh_absorption * rayleigh_density;
    const vec3 mie_absorption = atmosphere.mie_absorption * mie_density;

    const vec3 ozone_absorption = atmosphere.ozone_absorption * max(0.0f, 1.0f - abs(altitude_km - 25.0f) / 15.0f);

    media.extinction =
        media.rayleigh + rayleigh_absorption +
        media.mie + mie_absorption +
        ozone_absorption;
    
    return media;
}

vec3 calculate_transmittance(vec3 ro, vec3 rd, float len, vec3 center, AtmosphereSettings atmosphere) {
    // e^(-integral(extinction(x) * dx))
    vec3 total_extinction = vec3(0.0f);
    const float step_size = len / TRANSMITTANCE_STEPS;
    for (float i = 0.0f; i < TRANSMITTANCE_STEPS; i += 1.0f) {
        const vec3 x = ro + rd * step_size * i;
        const vec3 extinction = sample_media(x, center, atmosphere).extinction;
        total_extinction += extinction;
    }

    return exp(-total_extinction * step_size);
}

// https://ebruneton.github.io/precomputed_atmospheric_scattering/atmosphere/functions.glsl.html
float distance_to_atmosphere_top(AtmosphereSettings atmosphere, float r, float mu) {
    const float discriminant = r * r * (mu * mu - 1.0f) + atmosphere.atmosphere * atmosphere.atmosphere;
    
    return max(0.0f, -r * mu + sqrt(max(0.0, discriminant)));
}

vec2 transmittance_uv_from_r_mu(AtmosphereSettings atmosphere, float r, float mu) {
    const float H = sqrt(atmosphere.atmosphere * atmosphere.atmosphere - atmosphere.surface * atmosphere.surface);
    const float rho = sqrt(max(r * r - atmosphere.surface * atmosphere.surface, 0.0f));
    
    const float d = distance_to_atmosphere_top(atmosphere, r, mu);
    const float d_min = atmosphere.atmosphere - r;
    const float d_max = H + rho;
    
    const float x_mu = (d - d_min) / (d_max - d_min);
    const float x_r = rho / H;
    
    return vec2(x_mu, x_r);
}

vec2 transmittance_r_mu_from_uv(AtmosphereSettings atmosphere, vec2 uv) {
    const float x_mu = uv.x;
    const float x_r = uv.y;

    const float H = sqrt(atmosphere.atmosphere * atmosphere.atmosphere - atmosphere.surface * atmosphere.surface);
    const float rho = H * x_r;
    const float r = sqrt(rho * rho + atmosphere.surface * atmosphere.surface);

    const float d_min = atmosphere.atmosphere - r;
    const float d_max = H + rho;
    const float d = d_min + x_mu * (d_max - d_min);
    const float mu = clamp(d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0f * r * d), -1.0f, 1.0f);
    
    return vec2(r, mu);
}