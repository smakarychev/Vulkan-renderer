#version 460

layout(location = 0) in flat int instance_id;
layout(location = 1) in flat int triangle_id;

layout(location = 0) out uvec2 meshlet_triangle_id;

void main() {
    meshlet_triangle_id = uvec2(instance_id, triangle_id);
}