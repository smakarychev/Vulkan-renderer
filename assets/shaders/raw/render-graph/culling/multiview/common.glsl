#include "../../common.glsl"

#extension GL_EXT_shader_explicit_arithmetic_types: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8: require    

const uint VIEW_IS_ORTHOGRAPHIC_BIT = 0;
const uint VIEW_CLAMP_DEPTH_BIT = 1;
const uint VIEW_TRIANGLE_CULLING_BIT = 2;

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
    float width;
    float height;
    float hiz_width;
    float hiz_height;
    
    uint view_flags;
};

bool is_orthographic(ViewData view) {
    return ((view.view_flags >> VIEW_IS_ORTHOGRAPHIC_BIT) & 1u) == 1;
}

bool is_depth_clamped(ViewData view) {
    return ((view.view_flags >> VIEW_CLAMP_DEPTH_BIT) & 1u) == 1;
}

bool is_triangle_culled(ViewData view) {
    return ((view.view_flags >> VIEW_TRIANGLE_CULLING_BIT) & 1u) == 1;
}

struct ViewSpan {
    uint first;
    uint count;
};

const uint MAX_VIEWS = 64;
const uint MAX_GEOMETRIES = 4;