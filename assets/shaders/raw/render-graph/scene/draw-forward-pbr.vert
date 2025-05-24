#version 460

#include "../general/common.glsl"

#extension GL_ARB_shader_draw_parameters: enable

layout(set = 1, binding = 0) uniform camera_buffer {
    CameraGPU camera;
} u_camera;

layout(std430, set = 1, binding = 1) readonly buffer ugb_position {
    Position positions[];
} u_ugb_position;

layout(std430, set = 1, binding = 1) readonly buffer ugb_normal {
    Normal normals[];
} u_ugb_normal;

layout(std430, set = 1, binding = 1) readonly buffer ugb_tangent {
    Tangent tangents[];
} u_ugb_tangent;

layout(std430, set = 1, binding = 1) readonly buffer ugb_uv {
    UV uvs[];
} u_ugb_uv;

layout(scalar, set = 1, binding = 2) readonly buffer object_buffer {
    RenderObject objects[];
} u_objects;

layout(std430, set = 1, binding = 3) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

layout(location = 0) out uint vertex_material_id;
layout(location = 1) out vec3 vertex_position;
layout(location = 2) out vec3 vertex_normal;
layout(location = 3) out vec4 vertex_tangent;
layout(location = 4) out vec2 vertex_uv;
layout(location = 5) out float vertex_z_view;

void main() {
    const IndirectCommand command = u_commands.commands[gl_DrawIDARB];
    const RenderObject render_object = u_objects.objects[command.render_object];

    vertex_material_id = render_object.material_id;
    
    const mat4 model = render_object.model;
    
    const Position position = u_ugb_position.positions[render_object.position_index + gl_VertexIndex];
    const vec4 position_v = model * vec4(position.x, position.y, position.z, 1.0f);
    vertex_position = position_v.xyz;

    const Normal normal = u_ugb_normal.normals[render_object.normal_index + gl_VertexIndex];
    vertex_normal = transpose(inverse(mat3(model))) * vec3(normal.x, normal.y, normal.z);
    
    const Tangent tangent = u_ugb_tangent.tangents[render_object.tangent_index + gl_VertexIndex];
    vertex_tangent = vec4(tangent.x, tangent.y, tangent.z, tangent.w);
    
    const UV uv = u_ugb_uv.uvs[render_object.uv_index + gl_VertexIndex];
    vertex_uv = vec2(uv.u, uv.v);

    const vec4 view = u_camera.camera.view * position_v;
    vertex_z_view = view.z;
    gl_Position = u_camera.camera.view_projection * position_v;
}