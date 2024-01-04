#version 460

#include "common.shader_header"

layout(set = 0, binding = 0) uniform usampler2D u_visibility_texture;

layout (location = 0) in vec2 vert_uv;

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
    
    uint visibility_packed = texture(u_visibility_texture, vert_uv).r;
    VisibilityInfo visibility_info = unpack_visibility(visibility_packed);
    
    uint hash = murmur3(visibility_info.instance_id) ^ murmur3(visibility_info.triangle_id);
    out_color = vec4(hash & 255u, (hash >> 8) & 255u, (hash >> 16) & 255u, 255u) / 255;
}