#extension GL_EXT_scalar_block_layout: require
#extension GL_EXT_shader_16bit_storage: require

/* must have a scalar layout */
struct DirectionalLight {
    vec3 direction;
    vec3 color;
    float intensity;
    float size;
};

// todo: remove in favor of Sphere and Tube lights

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
};

struct LightsInfo {
    uint directional_light_count;
    uint point_light_count;
};

const uint VIEW_MAX_LIGHTS = 1024;
// each bin is 32 bits
const uint BIN_BIT_SIZE = 32;
const uint BIN_COUNT = VIEW_MAX_LIGHTS / BIN_BIT_SIZE;

struct Cluster {
    vec4 min;
    vec4 max;
    uint bins[BIN_COUNT];
};

struct Plane {
    // xyz components for normal, w for offset
    vec4 plane;
};

struct Tile {
    // tile is 4 planes, near and far are computed separately
    Plane planes[4];
    uint bins[BIN_COUNT];
};

Plane plane_by_points(vec3 a, vec3 b, vec3 c) {
    vec3 v0 = b - a;
    vec3 v1 = c - a;
    
    Plane plane;
    plane.plane.xyz = normalize(cross(v0, v1));
    plane.plane.w = dot(plane.plane.xyz, a);
    
    return plane;
}

struct ZBin {
    uint16_t min;
    uint16_t max;
};