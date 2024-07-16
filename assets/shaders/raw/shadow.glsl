/* must have a scalar layout */

const uint MAX_CASCADES = 5;

struct CSMData {
    uint cascade_count;
    float cascades[MAX_CASCADES];
    mat4 view_projections[MAX_CASCADES];
    mat4 views[MAX_CASCADES];
    float near[MAX_CASCADES];
    float far[MAX_CASCADES];
};