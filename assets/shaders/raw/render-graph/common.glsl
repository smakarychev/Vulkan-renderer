#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_16bit_storage: require

struct Material {
    vec4 albedo_color;
    float metallic;
    float roughness;
    float pad0;
    uint albedo_texture_index;
    uint normal_texture_index;
    uint metallic_roughness_texture_index;
    uint ambient_occlusion_texture_index;
    uint emissive_texture_index;
};

struct IndirectCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;

    uint render_object;
};

struct VkDispatchIndirectCommand {
    uint x;
    uint y;
    uint z;
};

struct object_data {
    mat4 model;
    // bounding sphere
    float x;
    float y;
    float z;
    float r;
};

struct Meshlet {
    int8_t cone_x;
    int8_t cone_y;
    int8_t cone_z;
    int8_t cone_cutoff;
    // bounding sphere
    float x;
    float y;
    float z;
    float r;
};

struct Position {
    float x;
    float y;
    float z;
};

struct Normal {
    float x;
    float y; 
    float z;
};

struct Tangent {
    float x;
    float y; 
    float z;
};

struct UV {
    float u;
    float v;
};

struct Triangle {
    vec4 a;
    vec4 b;
    vec4 c;
};

vec4 vec4_from_position(Position position) {
    return vec4(position.x, position.y, position.z, 1.0f);
}

#include "tonemapping.glsl"