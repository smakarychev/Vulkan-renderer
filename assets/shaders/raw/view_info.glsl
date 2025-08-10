#extension GL_EXT_scalar_block_layout: require

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


    mat4 prev_view_projection;
    mat4 prev_projection;
    mat4 prev_view;

    vec3 prev_position;
    float prev_near;
    vec3 prev_forward;
    float prev_far;

    mat4 prev_inv_view_projection;
    mat4 prev_inv_projection;
    mat4 prev_inv_view;

    float prev_frustum_top_y;
    float prev_frustum_top_z;
    float prev_frustum_right_x;
    float prev_frustum_right_z;
    float prev_frustum_near;
    float prev_frustum_far;
    float prev_projection_width;
    float prev_projection_height;
    float prev_projection_bias_x;
    float prev_projection_bias_y;

    vec2 prev_resolution;
    vec2 prev_hiz_resolution;
    uint prev_view_flags;
    uint prev_unused_0;

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
    float max_light_cull_distance;
    float volumetric_cloud_shadow_strength; 
    uint transmittance_lut;
    uint sky_view_lut;
    uint volumetric_cloud_shadow;
    uint padding[1];

    mat4 volumetric_cloud_view_projection;
    mat4 volumetric_cloud_view;
    
    float frame_number;
    uint frame_number_u32;
};