float extract_scale(mat4 matrix) {
    vec3 scales = vec3(dot(matrix[0], matrix[0]), dot(matrix[1], matrix[1]), dot(matrix[2], matrix[2]));
    return max(scales.x, max(scales.y, scales.z));
}

bool is_backface_meshlet_visible(vec3 sphere_origin, float radius, vec3 cone_axis, float cone_cutoff) {
    return dot(sphere_origin, cone_axis) < cone_cutoff * length(sphere_origin) + radius;
}

bool is_frustum_visible(vec3 sphere_origin, float radius, scene_data scene) {
    bool visible = true;
    visible = visible && abs(scene.frustum_right_x * sphere_origin.x) < -sphere_origin.z * scene.frustum_right_z + radius;
    visible = visible && abs(scene.frustum_top_y * sphere_origin.y) < -sphere_origin.z * scene.frustum_top_z + radius;
    visible = visible &&
        sphere_origin.z - radius <= -scene.frustum_near &&
        sphere_origin.z + radius >= -scene.frustum_far;
    
    return visible;
}

bool is_occlusion_visible(vec3 sphere_origin, float radius, scene_data scene, sampler hiz_sampler, texture2D hiz) {
    if (sphere_origin.z + radius >= -scene.frustum_near)
        return true;
    
    vec3 cr = sphere_origin * radius;
    float czr2 = sphere_origin.z * sphere_origin.z - radius * radius;

    float vx = sqrt(sphere_origin.x * sphere_origin.x + czr2);
    float minx = (vx * sphere_origin.x - cr.z) / (vx * sphere_origin.z + cr.x);
    float maxx = (vx * sphere_origin.x + cr.z) / (vx * sphere_origin.z - cr.x);

    float vy = sqrt(sphere_origin.y * sphere_origin.y + czr2);
    float miny = (vy * sphere_origin.y - cr.z) / (vy * sphere_origin.z + cr.y);
    float maxy = (vy * sphere_origin.y + cr.z) / (vy * sphere_origin.z - cr.y);

    vec4 aabb = vec4(minx, miny, maxx, maxy) *
        vec4(scene.projection_width, scene.projection_height, scene.projection_width, scene.projection_height);
    // clip space -> uv space
    aabb = aabb.xwzy * vec4(-0.5f, 0.5f, -0.5f, 0.5f) + vec4(0.5f);

    float width =  (aabb.x - aabb.z) * scene.hiz_width;
    float height = (aabb.y - aabb.w) * scene.hiz_height;

    float level = ceil(log2(max(width, height)));

    float depth = textureLod(sampler2D(hiz, hiz_sampler), (aabb.xy + aabb.zw) * 0.5f, level).r;

    float coeff = 1.0f / (scene.frustum_far - scene.frustum_near);
    float projected_depth = coeff *
        (-scene.frustum_far * scene.frustum_near / (sphere_origin.z + radius) -
        scene.frustum_near);

    return projected_depth >= depth;
}

bool is_occlusion_visible_orthographic(vec3 sphere_origin, float radius, scene_data scene, sampler hiz_sampler, 
    texture2D hiz) {

    if (sphere_origin.z + radius >= -scene.frustum_near)
        return true;
    
    float minx = sphere_origin.x - radius;
    float maxx = sphere_origin.x + radius;
    float miny = sphere_origin.y - radius;
    float maxy = sphere_origin.y + radius;

    vec4 aabb = vec4(minx, miny, maxx, maxy) * 
        vec4(scene.projection_width, scene.projection_height, scene.projection_width, scene.projection_height) +
        vec4(scene.projection_bias_x, scene.projection_bias_y, scene.projection_bias_x, scene.projection_bias_y);
    // clip space -> uv space
    aabb = aabb.xyzw * vec4(0.5f, -0.5f, 0.5f, -0.5f) + vec4(0.5f);

    float width =  (aabb.z - aabb.x) * scene.hiz_width;
    float height = (aabb.y - aabb.w) * scene.hiz_height;

    float level = ceil(log2(max(width, height)));

    float depth = textureLod(sampler2D(hiz, hiz_sampler), (aabb.xy + aabb.zw) * 0.5f, level).r;

    float coeff = 1.0f / (scene.frustum_far - scene.frustum_near);
    float projected_depth = coeff * ((sphere_origin.z + radius) + scene.frustum_far);

    return projected_depth >= depth;
}

bool is_backface_triangle_visible(Triangle triangle) {
    return determinant(mat3(triangle.a.xyw, triangle.b.xyw, triangle.c.xyw)) <= 0;
}

bool is_screen_size_visible(vec4 aabb, float width, float height) {
    aabb = aabb * vec4(width, height, width, height);
    return round(aabb.x) != round(aabb.z) && round(aabb.y) != round(aabb.w);
}

bool is_frustum_triangle_visible(vec4 aabb) {
    return aabb.z >= 0 && aabb.x <= 1 && aabb.y >= 0 && aabb.w <= 1;
}

bool is_occlusion_triangle_visible(vec4 aabb, float z, scene_data scene, sampler hiz_sampler, texture2D hiz) {
    float width = (aabb.z - aabb.x) * scene.hiz_width;
    float height = (aabb.y - aabb.w) * scene.hiz_height;
    float level = ceil(log2(max(width, height)));

    float depth = textureLod(sampler2D(hiz, hiz_sampler),(aabb.xy + aabb.zw) * 0.5, level).r;

    return depth <= z;
}

bool flags_visible(uint8_t flags) {
    return (flags & 1) == uint8_t(1);
}

void flags_set_visible(inout uint8_t flags) {
    flags |= uint8_t(1);
}

void flags_unset_visible(inout uint8_t flags) {
    flags &= uint8_t(~1);
}

bool flags_should_draw(uint8_t flags) {
    return ((flags >> 1) & 1) == uint8_t(1);
}

void set_flags_set_should_draw(inout uint8_t flags) {
    flags |= uint8_t(1 << 1);
}

void set_flags_unset_should_draw(inout uint8_t flags) {
    flags &= uint8_t(~(1 << 1));
}

bool flags_triangle_culled(uint8_t flags) {
    return ((flags >> 2) & 1) != uint8_t(1);
}

void flags_set_should_triangle_cull(inout uint8_t flags) {
    flags |= uint8_t(1 << 2);
}

void flags_unset_should_triangle_cull(inout uint8_t flags) {
    flags &= uint8_t(~(1 << 2));
}