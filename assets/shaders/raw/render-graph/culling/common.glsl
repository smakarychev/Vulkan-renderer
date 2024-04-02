#include "../common.glsl"

#extension GL_EXT_shader_explicit_arithmetic_types: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8: require    
#extension GL_EXT_shader_atomic_int64: require

struct scene_data {
    mat4 view_matrix;
    float frustum_top_y;
    float frustum_top_z;
    float frustum_right_x;
    float frustum_right_z;
    float frustum_near;
    float frustum_far;
    float projection_width;
    float projection_height;
    float hiz_width;
    float hiz_height;
    
    uint pad0;
    uint pad1;
};