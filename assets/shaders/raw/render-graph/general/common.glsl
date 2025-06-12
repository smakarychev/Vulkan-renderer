#include "../common.glsl"
#include "../../view_info.glsl"

struct VisibilityInfo {
    uint instance_id;
    uint triangle_id;
};

uint pack_visibility(VisibilityInfo visibility_info) {
    return (visibility_info.instance_id << TRIANGLE_BITS) | visibility_info.triangle_id;
}

VisibilityInfo unpack_visibility(uint packed) {
    VisibilityInfo visibility_info;
    visibility_info.triangle_id = packed & TRIANGLE_MASK;
    visibility_info.instance_id = packed >> TRIANGLE_BITS;
    return visibility_info;
}
