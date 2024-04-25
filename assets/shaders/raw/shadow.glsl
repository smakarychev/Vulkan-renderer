/* must have a scalar layout */

const uint MAX_CASCADES = 5;

struct CSMData {
    uint cascade_count;
    float cascades[MAX_CASCADES];
    mat4 view_projections[MAX_CASCADES];
};