const uint VIEW_IS_ORTHOGRAPHIC_BIT = 0;
const uint VIEW_CLAMP_DEPTH_BIT = 1;

struct ViewInfo {
    mat4 view_projection;
    mat4 projection;
    mat4 view;

    vec3 position;
    float near;
    vec3 forward;
    float far;

    mat4 inv_view_projection;
    mat4 inv_projection;
    mat4 inv_view;

    float frustum_top_y;
    float frustum_top_z;
    float frustum_right_x;
    float frustum_right_z;
    float frustum_near;
    float frustum_far;
    float projection_width;
    float projection_height;
    float projection_bias_x;
    float projection_bias_y;
    
    vec2 resolution;
    vec2 hiz_resolution;
    uint view_flags;
    uint unused_0;
    

    vec4 rayleigh_scattering;
    vec4 rayleigh_absorption;
    vec4 mie_scattering;
    vec4 mie_absorption;
    vec4 ozone_absorption;
    vec4 surface_albedo;

    float surface;
    float atmosphere;
    float rayleigh_density;
    float mie_density;
    float ozone_density;

    

    float environment_power;
    bool soft_shadows;
    uint transmittance_lut;
};