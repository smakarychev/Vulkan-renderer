#version 460

#include "material.glsl"

layout(std430, set = 2, binding = 0) readonly buffer material_buffer{
    Material materials[];
} u_materials;

@bindless
layout(set = 2, binding = 1) uniform texture2D u_textures[];

void main() {
    // this is just the easiest way to create material descriptors
}