uvec3 get_indices(IndirectCommand command, uint triangle_id) {
    uvec3 indices = uvec3(
        command.vertexOffset + uint(u_indices.indices[command.firstIndex + triangle_id * 3 + 0]),
        command.vertexOffset + uint(u_indices.indices[command.firstIndex + triangle_id * 3 + 1]),
        command.vertexOffset + uint(u_indices.indices[command.firstIndex + triangle_id * 3 + 2]));

    return indices;
}

Triangle get_triangle_local(uint position_offset, uvec3 indices) {
    Triangle triangle;
    triangle.a = vec4_from_position(u_ugb_position.positions[position_offset + indices.x]);
    triangle.b = vec4_from_position(u_ugb_position.positions[position_offset + indices.y]);
    triangle.c = vec4_from_position(u_ugb_position.positions[position_offset + indices.z]);

    return triangle;
}

void transform_to_clip_space(RenderObject object, inout Triangle triangle, VisibilityInfo visibility_info) {
    triangle.a = u_view_info.view.view_projection * object.model * triangle.a;
    triangle.b = u_view_info.view.view_projection * object.model * triangle.b;
    triangle.c = u_view_info.view.view_projection * object.model * triangle.c;
}

mat3x2 get_uvs(uint uv_offset, uvec3 indices) {
    const vec2 uv_a = vec2(u_ugb_uv.uvs[uv_offset + indices.x].u, u_ugb_uv.uvs[uv_offset + indices.x].v);
    const vec2 uv_b = vec2(u_ugb_uv.uvs[uv_offset + indices.y].u, u_ugb_uv.uvs[uv_offset + indices.y].v);
    const vec2 uv_c = vec2(u_ugb_uv.uvs[uv_offset + indices.z].u, u_ugb_uv.uvs[uv_offset + indices.z].v);

    return mat3x2(uv_a, uv_b, uv_c);
}

mat3 get_normals(uint normal_offset, uvec3 indices) {
    const vec3 normal_a = vec3(
        u_ugb_normal.normals[normal_offset + indices.x].x,
        u_ugb_normal.normals[normal_offset + indices.x].y,
        u_ugb_normal.normals[normal_offset + indices.x].z);
    const vec3 normal_b = vec3(
        u_ugb_normal.normals[normal_offset + indices.y].x,
        u_ugb_normal.normals[normal_offset + indices.y].y,
        u_ugb_normal.normals[normal_offset + indices.y].z);
    const vec3 normal_c = vec3(
        u_ugb_normal.normals[normal_offset + indices.z].x,
        u_ugb_normal.normals[normal_offset + indices.z].y,
        u_ugb_normal.normals[normal_offset + indices.z].z);

    return mat3(normal_a, normal_b, normal_c);
}

mat3 get_tangents(uint tangent_offset, uvec3 indices) {
    const vec3 tangent_a = vec3(
        u_ugb_tangent.tangents[tangent_offset + indices.x].x,
        u_ugb_tangent.tangents[tangent_offset + indices.x].y,
        u_ugb_tangent.tangents[tangent_offset + indices.x].z);
    const vec3 tangent_b = vec3(
        u_ugb_tangent.tangents[tangent_offset + indices.y].x,
        u_ugb_tangent.tangents[tangent_offset + indices.y].y,
        u_ugb_tangent.tangents[tangent_offset + indices.y].z);
    const vec3 tangent_c = vec3(
        u_ugb_tangent.tangents[tangent_offset + indices.z].x,
        u_ugb_tangent.tangents[tangent_offset + indices.z].y,
        u_ugb_tangent.tangents[tangent_offset + indices.z].z);

    return mat3(tangent_a, tangent_b, tangent_c);
}

void convert_to_world_space_normal(inout vec3 normal, VisibilityInfo visibility_info) {
    IndirectCommand command = u_commands.commands[visibility_info.instance_id];
    RenderObject object = u_objects.objects[command.render_object];

    normal = normalize((transpose(inverse(object.model)) * vec4(normal, 0.0f)).xyz);
}

void convert_to_world_space_normal_tangents(
    inout vec3 normal, vec3 normal_dx, vec3 normal_dy,
    inout vec3 tangent, vec3 tangent_dx, vec3 tangent_dy,
    inout vec3 bitangent, inout vec3 bitangent_dx, inout vec3 bitangent_dy, VisibilityInfo visibility_info) {
    IndirectCommand command = u_commands.commands[visibility_info.instance_id];
    RenderObject object = u_objects.objects[command.render_object];

    tangent = normalize((transpose(inverse(object.model)) * vec4(tangent, 0.0f)).xyz);
    normal = normalize((transpose(inverse(object.model)) * vec4(normal, 0.0f)).xyz);
    // re-orthogonalize
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    bitangent = normalize(cross(normal, tangent));

    bitangent_dx = cross(normal_dx, tangent) - cross(tangent_dx, normal);
    bitangent_dy = cross(normal_dy, tangent) - cross(tangent_dy, normal);
}

struct InterpolationData {
    vec3 barycentric;
    vec3 ddx;
    vec3 ddy;
};

InterpolationData get_interpolation_data(Triangle triangle, vec2 screen_pos, vec2 resolution) {
    const vec3 one_over_w = 1.0f / vec3(triangle.a.w, triangle.b.w, triangle.c.w);

    const vec2 ndc_a = triangle.a.xy * one_over_w.x;
    const vec2 ndc_b = triangle.b.xy * one_over_w.y;
    const vec2 ndc_c = triangle.c.xy * one_over_w.z;

    const float inverse_det = 1.0f / determinant(mat2(ndc_c - ndc_b, ndc_a - ndc_b));
    vec3 ddx = vec3(ndc_b.y - ndc_c.y, ndc_c.y - ndc_a.y, ndc_a.y - ndc_b.y) * inverse_det * one_over_w;
    vec3 ddy = vec3(ndc_c.x - ndc_b.x, ndc_a.x - ndc_c.x, ndc_b.x - ndc_a.x) * inverse_det * one_over_w;
    float ddx_sum = dot(ddx, vec3(1.0f, 1.0f, 1.0f));
    float ddy_sum = dot(ddy, vec3(1.0f, 1.0f, 1.0f));

    const vec2 delta_vec = screen_pos - ndc_a;
    const float interpolated_inv_w = one_over_w.x + delta_vec.x * ddx_sum + delta_vec.y * ddy_sum;
    const float interpolated_w = 1.0f / interpolated_inv_w;

    vec3 lambda;
    lambda.x = interpolated_w * (one_over_w.x + delta_vec.x * ddx.x + delta_vec.y * ddy.x);
    lambda.y = interpolated_w * (0.0f         + delta_vec.x * ddx.y + delta_vec.y * ddy.y);
    lambda.z = interpolated_w * (0.0f         + delta_vec.x * ddx.z + delta_vec.y * ddy.z);
    // todo: isn't it better?
    //lambda.z = 1.0 - lambda.x - lambda.y; 

    ddx *= (2.0f / resolution.x);
    ddy *= (2.0f / resolution.y);
    ddx_sum *= (2.0f / resolution.x);
    ddy_sum *= (2.0f / resolution.y);

    const float interpolated_w_ddx = 1.0f / (interpolated_inv_w + ddx_sum);
    const float interpolated_w_ddy = 1.0f / (interpolated_inv_w + ddy_sum);

    ddx = interpolated_w_ddx * (lambda * interpolated_inv_w + ddx) - lambda;
    ddy = interpolated_w_ddy * (lambda * interpolated_inv_w + ddy) - lambda;

    InterpolationData interpolation_data;
    interpolation_data.barycentric = lambda;
    interpolation_data.ddx = ddx;
    interpolation_data.ddy = ddy;

    return interpolation_data;
}

float interpolate(InterpolationData interpolation, vec3 attribute_vec) {
    return dot(attribute_vec, interpolation.barycentric);
}

float interpolate(InterpolationData interpolation, float a, float b, float c) {
    return interpolate(interpolation, vec3(a, b, c));
}

vec3 interpolate_with_derivatives(InterpolationData interpolation, vec3 attribute_vec) {
    vec3 result;
    result[0] = dot(attribute_vec, interpolation.barycentric);
    result[1] = dot(attribute_vec, interpolation.ddx);
    result[2] = dot(attribute_vec, interpolation.ddy);

    return result;
}

mat3x2 interpolate_with_derivatives_2d(InterpolationData interpolation, mat3x2 attributes) {
    const vec3 attribute_x = vec3(attributes[0].x, attributes[1].x, attributes[2].x);
    const vec3 attribute_y = vec3(attributes[0].y, attributes[1].y, attributes[2].y);

    const vec3 attribute_x_interpolated = interpolate_with_derivatives(interpolation, attribute_x);
    const vec3 attribute_y_interpolated = interpolate_with_derivatives(interpolation, attribute_y);

    const float ax = attribute_x_interpolated[0];
    const float ay = attribute_y_interpolated[0];

    const float addxx = attribute_x_interpolated[1];
    const float addxy = attribute_y_interpolated[1];

    const float addyx = attribute_x_interpolated[2];
    const float addyy = attribute_y_interpolated[2];

    return mat3x2(vec2(ax, ay), vec2(addxx, addxy), vec2(addyx, addyy));
}

vec3 interpolate_3d(InterpolationData interpolation, mat3 attributes) {
    const vec3 attribute_x = vec3(attributes[0].x, attributes[1].x, attributes[2].x);
    const vec3 attribute_y = vec3(attributes[0].y, attributes[1].y, attributes[2].y);
    const vec3 attribute_z = vec3(attributes[0].z, attributes[1].z, attributes[2].z);

    const float attribute_x_interpolated = interpolate(interpolation, attribute_x);
    const float attribute_y_interpolated = interpolate(interpolation, attribute_y);
    const float attribute_z_interpolated = interpolate(interpolation, attribute_z);

    return vec3(attribute_x_interpolated, attribute_y_interpolated, attribute_z_interpolated);
}

mat3 interpolate_with_derivatives_3d(InterpolationData interpolation, mat3 attributes) {
    const vec3 attribute_x = vec3(attributes[0].x, attributes[1].x, attributes[2].x);
    const vec3 attribute_y = vec3(attributes[0].y, attributes[1].y, attributes[2].y);
    const vec3 attribute_z = vec3(attributes[0].z, attributes[1].z, attributes[2].z);

    const vec3 attribute_x_interpolated = interpolate_with_derivatives(interpolation, attribute_x);
    const vec3 attribute_y_interpolated = interpolate_with_derivatives(interpolation, attribute_y);
    const vec3 attribute_z_interpolated = interpolate_with_derivatives(interpolation, attribute_z);

    const float ax = attribute_x_interpolated[0];
    const float ay = attribute_y_interpolated[0];
    const float az = attribute_z_interpolated[0];

    const float addxx = attribute_x_interpolated[1];
    const float addxy = attribute_y_interpolated[1];
    const float addxz = attribute_z_interpolated[1];

    const float addyx = attribute_x_interpolated[2];
    const float addyy = attribute_y_interpolated[2];
    const float addyz = attribute_z_interpolated[2];

    return mat3(vec3(ax, ay, az), vec3(addxx, addxy, addxz), vec3(addyx, addyy, addyz));
}


// the interpolated data (does not actually come from gbuffer)
struct GBufferData {
    vec3 position;
    vec2 uv;
    vec2 uv_dx;
    vec2 uv_dy;
    vec3 normal;
    vec3 flat_normal;
    vec3 albedo;
    vec3 emissive;
    vec2 metallic_roughness;
    float ao;
    float z_view;
    float depth;
};

GBufferData get_gbuffer_data(VisibilityInfo visibility_info) {
    const IndirectCommand command = u_commands.commands[visibility_info.instance_id];
    const uvec3 indices = get_indices(command, visibility_info.triangle_id);
    const RenderObject object = u_objects.objects[command.render_object];
    
    Triangle triangle = get_triangle_local(object.position_index, indices);
    transform_to_clip_space(object, triangle, visibility_info);
    const InterpolationData interpolation_data = get_interpolation_data(triangle, vertex_position, u_view_info.view.resolution);

    const float w = dot(vec3(triangle.a.w, triangle.b.w, triangle.c.w), interpolation_data.barycentric);
    const float z = -w * u_view_info.view.projection[2][2] + u_view_info.view.projection[3][2];
    vec4 position = u_view_info.view.inv_projection * vec4(vertex_position * w, z, w);
    const float z_view = position.z;
    position = u_view_info.view.inv_view * position;

    const mat3x2 uvs = get_uvs(object.uv_index, indices);
    const mat3x2 uvs_interpolated = interpolate_with_derivatives_2d(interpolation_data, uvs);
    const vec2 uv = vec2(uvs_interpolated[0][0], uvs_interpolated[0][1]);
    const vec2 uv_dx = vec2(uvs_interpolated[1][0], uvs_interpolated[1][1]);
    const vec2 uv_dy = vec2(uvs_interpolated[2][0], uvs_interpolated[2][1]);

    const Material material = u_materials.materials[object.material_id];

    vec3 albedo = material.albedo_color.rgb;
    albedo *= textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.albedo_texture_index], u_sampler)), uv, uv_dx, uv_dy).rgb;

    const vec3 emissive = textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.emissive_texture_index], u_sampler)), uv, uv_dx, uv_dy).rgb;

    const vec2 metallic_roughness = textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.metallic_roughness_texture_index], u_sampler)), uv, uv_dx, uv_dy).bg;

    const float ao = textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.ambient_occlusion_texture_index], u_sampler)), uv, uv_dx, uv_dy).r;

    const mat3 normals = get_normals(object.normal_index, indices);
    const mat3 normals_interpolated = interpolate_with_derivatives_3d(interpolation_data, normals);
    const vec3 flat_normal = vec3(normals_interpolated[0][0], normals_interpolated[0][1], normals_interpolated[0][2]);
    vec3 normal = flat_normal;
    const vec3 normal_dx = vec3(normals_interpolated[1][0], normals_interpolated[1][1], normals_interpolated[1][2]);
    const vec3 normal_dy = vec3(normals_interpolated[2][0], normals_interpolated[2][1], normals_interpolated[2][2]);

    const mat3 tangents = get_tangents(object.tangent_index, indices);
    const mat3 tangents_interpolated = interpolate_with_derivatives_3d(interpolation_data, tangents);
    vec3 tangent = vec3(tangents_interpolated[0][0], tangents_interpolated[0][1], tangents_interpolated[0][2]);
    const vec3 tangent_dx = vec3(tangents_interpolated[1][0], tangents_interpolated[1][1], tangents_interpolated[1][2]);
    const vec3 tangent_dy = vec3(tangents_interpolated[2][0], tangents_interpolated[2][1], tangents_interpolated[2][2]);
    vec3 bitangent;
    vec3 bitangent_dx;
    vec3 bitangent_dy;
    convert_to_world_space_normal_tangents(
        normal, normal_dx, normal_dy,
        tangent, tangent_dx, tangent_dy,
        bitangent, bitangent_dx, bitangent_dy,
        visibility_info);

    vec3 normal_from_map = textureGrad(nonuniformEXT(sampler2D(u_textures[
        material.normal_texture_index], u_sampler)), uv, uv_dx, uv_dy).rgb;
    normal_from_map = normalize(normal_from_map * 2.0f - 1.0f);
    normal = tangent * normal_from_map.x + bitangent * normal_from_map.y + normal * normal_from_map.z;

    GBufferData data;
    data.position = position.xyz;
    data.uv = uv;
    data.uv_dx = uv_dx;
    data.uv_dy = uv_dy;
    data.normal = normal;
    data.flat_normal = flat_normal;
    data.albedo = albedo;
    data.emissive = emissive;
    data.metallic_roughness = vec2(material.metallic, material.roughness) * metallic_roughness;
    data.metallic_roughness.g = max(data.metallic_roughness.g, 0.015625f);
    data.ao = ao;
    data.z_view = z_view;
    data.depth = z / w;

    return data;
}
