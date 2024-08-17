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

struct Cluster {
    vec4 min;
    vec4 max;
};