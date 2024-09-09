#extension GL_EXT_scalar_block_layout: require

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