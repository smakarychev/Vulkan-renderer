#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 vert_normal;
layout(location = 1) in vec2 vert_uv;
layout(location = 2) in flat uint vert_command_id;
layout(location = 3) in flat uint vert_triangle_offset;
layout(location = 4) in vec3 vert_pos;

@dynamic
layout(set = 0, binding = 1) uniform scene_data{
    vec4 fog_color;          // w is for exponent
    vec4 fog_distances;      // x for min, y for max, zw unused.
    vec4 ambient_color;
    vec4 sunlight_direction; // w for sun power
    vec4 sunlight_color;
} u_scene_data;

@bindless
layout(set = 1, binding = 2) uniform texture2D u_textures[];

@immutable_sampler
layout(set = 0, binding = 2) uniform sampler u_sampler;

struct Material {
    vec4 albedo_color;
    uint albedo_texture_index;
    uint pad0;
    uint pad1;
    uint pad2;
};

struct IndirectCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;

    uint render_object;
};

layout(std430, set = 2, binding = 1) readonly buffer material_buffer{
    Material materials[];
} u_material_buffer;

@dynamic
layout(std430, set = 2, binding = 2) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_command_buffer;

@dynamic
layout(std430, set = 2, binding = 3) readonly buffer triangle_buffer {
    uint triangles[];
} u_triangle_buffer;


layout(location = 0) out vec4 out_color;

uint rotl(uint x, uint r) {
    return (x << r) | (x >> (32u - r));
}

uint fmix(uint h)
{
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

uint murmur3(uint seed) {
    const uint c1 = 0xcc9e2d51u;
    const uint c2 = 0x1b873593u;
    
    uint h = 0u;
    uint k = seed;

    k *= c1;
    k = rotl(k,15u);
    k *= c2;

    h ^= k;
    h = rotl(h,13u);
    h = h*5u+0xe6546b64u;

    h ^= 4u;

    return fmix(h);
}

void main() {
    IndirectCommand command = u_command_buffer.commands[vert_command_id]; 
    uint object_index = command.render_object;
    Material material = u_material_buffer.materials[object_index];
    //if (material.albedo_texture_index != -1)
    //    out_color = texture(nonuniformEXT(sampler2D(u_textures[nonuniformEXT(material.albedo_texture_index)], u_sampler)), vert_uv);
    //else
    //    out_color = material.albedo_color;
    //
    //if (out_color.a < 0.5f)
    //    discard;
    
    //out_color = vec4(out_color.rgb * dot(normalize(vert_normal), normalize(vec3(u_scene_data.sunlight_direction))), out_color.a);
    uint triangle_id = u_triangle_buffer.triangles[vert_triangle_offset + gl_PrimitiveID];
    uint instance_id = command.firstInstance;
    uint hash = murmur3(instance_id) ^ murmur3(triangle_id);
    //uint hash = murmur3(instance_id);
    //hash = triangle_id;
    //out_color = vec4(hash & 255u, (hash >> 8) & 255u, (hash >> 16) & 255u, 255u) / 255;
    out_color = vec4(dFdy(vert_uv.x), dFdy(vert_uv.y), 0.0, 1.0);
    //out_color = vec4(vert_uv.x, vert_uv.y, 0.0, 1.0);
}