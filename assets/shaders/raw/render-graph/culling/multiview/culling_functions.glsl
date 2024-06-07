float extract_scale(mat4 matrix) {
    const vec3 scales = vec3(dot(matrix[0], matrix[0]), dot(matrix[1], matrix[1]), dot(matrix[2], matrix[2]));
    return max(scales.x, max(scales.y, scales.z));
}

bool is_backface_meshlet_visible(vec3 sphere_origin, float radius, vec3 cone_axis, float cone_cutoff) {
    return dot(sphere_origin, cone_axis) < cone_cutoff * length(sphere_origin) + radius;
}

bool is_frustum_visible(vec3 sphere_origin, float radius, ViewData view) {
    bool visible = true;
    visible = visible && abs(view.frustum_right_x * sphere_origin.x) < -sphere_origin.z * view.frustum_right_z + radius;
    visible = visible && abs(view.frustum_top_y * sphere_origin.y) < -sphere_origin.z * view.frustum_top_z + radius;
    visible = visible &&
        sphere_origin.z - radius <= -view.frustum_near &&
        sphere_origin.z + radius >= -view.frustum_far;
    
    return visible;
}

bool is_occlusion_visible(vec3 sphere_origin, float radius, ViewData view, sampler hiz_sampler, texture2D hiz) {
    if (sphere_origin.z + radius >= -view.frustum_near)
        return true;

    const vec3 cr = sphere_origin * radius;
    const float czr2 = sphere_origin.z * sphere_origin.z - radius * radius;

    const float vx = sqrt(sphere_origin.x * sphere_origin.x + czr2);
    const float minx = (vx * sphere_origin.x - cr.z) / (vx * sphere_origin.z + cr.x);
    const float maxx = (vx * sphere_origin.x + cr.z) / (vx * sphere_origin.z - cr.x);

    const float vy = sqrt(sphere_origin.y * sphere_origin.y + czr2);
    const float miny = (vy * sphere_origin.y - cr.z) / (vy * sphere_origin.z + cr.y);
    const float maxy = (vy * sphere_origin.y + cr.z) / (vy * sphere_origin.z - cr.y);

    vec4 aabb = vec4(minx, miny, maxx, maxy) *
        vec4(view.projection_width, view.projection_height, view.projection_width, view.projection_height);
    // clip space -> uv space
    aabb = aabb.xwzy * vec4(-0.5f, 0.5f, -0.5f, 0.5f) + vec4(0.5f);

    const float width =  (aabb.x - aabb.z) * view.hiz_width;
    const float height = (aabb.y - aabb.w) * view.hiz_height;

    const float level = ceil(log2(max(width, height)));

    const float depth = textureLod(sampler2D(hiz, hiz_sampler), (aabb.xy + aabb.zw) * 0.5f, level).r;

    const float coeff = 1.0f / (view.frustum_far - view.frustum_near);
    const float projected_depth = coeff *
        (-view.frustum_far * view.frustum_near / (sphere_origin.z + radius) -
        view.frustum_near);

    return projected_depth >= depth;
}

bool is_occlusion_visible_orthographic(vec3 sphere_origin, float radius, ViewData view, sampler hiz_sampler, 
    texture2D hiz) {

    if (sphere_origin.z + radius >= -view.frustum_near)
        return true;
    
    const float minx = sphere_origin.x - radius;
    const float maxx = sphere_origin.x + radius;
    const float miny = sphere_origin.y - radius;
    const float maxy = sphere_origin.y + radius;

    vec4 aabb = vec4(minx, miny, maxx, maxy) * 
        vec4(view.projection_width, view.projection_height, view.projection_width, view.projection_height) +
        vec4(view.projection_bias_x, view.projection_bias_y, view.projection_bias_x, view.projection_bias_y);
    // clip space -> uv space
    aabb = aabb.xyzw * vec4(0.5f, -0.5f, 0.5f, -0.5f) + vec4(0.5f);

    const float width =  (aabb.z - aabb.x) * view.hiz_width;
    const float height = (aabb.y - aabb.w) * view.hiz_height;

    const float level = ceil(log2(max(width, height)));

    const float depth = textureLod(sampler2D(hiz, hiz_sampler), (aabb.xy + aabb.zw) * 0.5f, level).r;

    const float coeff = 1.0f / (view.frustum_far - view.frustum_near);
    const float projected_depth = coeff * ((sphere_origin.z + radius) + view.frustum_far);

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

bool is_occlusion_triangle_visible(vec4 aabb, float z, ViewData view, sampler hiz_sampler, texture2D hiz) {
    const float width = (aabb.z - aabb.x) * view.hiz_width;
    const float height = (aabb.y - aabb.w) * view.hiz_height;
    const float level = ceil(log2(max(width, height)));

    const float depth = textureLod(sampler2D(hiz, hiz_sampler),(aabb.xy + aabb.zw) * 0.5, level).r;

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