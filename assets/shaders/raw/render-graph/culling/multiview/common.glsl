#include "../../common.glsl"

#extension GL_EXT_shader_explicit_arithmetic_types: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8: require    
#extension GL_EXT_shader_atomic_int64: require

struct ViewData {
    mat4 view_matrix;
    mat4 view_projection_matrix;
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
    float hiz_width;
    float hiz_height;
    
    bool is_orthographic;
    bool clamp_depth;
};

struct ViewSpan {
    uint first;
    uint count;
};

const uint MAX_VIEWS = 64;
const uint MAX_GEOMETRIES = 3;