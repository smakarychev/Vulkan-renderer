#include "../common.glsl"
#include "../../camera.glsl"

struct VisibilityInfo {
    uint instance_id;
    uint triangle_id;
};

uint pack_visibility(VisibilityInfo visibility_info) {
    return (visibility_info.instance_id << TRIANGLE_BITS) | visibility_info.triangle_id;
}

VisibilityInfo unpack_visibility(uint packed) {
    VisibilityInfo visibilityInfo;
    visibilityInfo.triangle_id = packed & TRIANGLE_MASK;
    visibilityInfo.instance_id = packed >> TRIANGLE_BITS;
    return visibilityInfo;
}