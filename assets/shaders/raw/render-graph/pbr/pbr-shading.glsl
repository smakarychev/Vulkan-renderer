vec3 shade_pbr_point_light(ShadeInfo shade_info, uint light_index) {
    const PointLight light = u_point_lights.lights[light_index];

    vec3 light_dir = light.position - shade_info.position;
    const float distance2 = dot(light_dir, light_dir);
    const float falloff = pbr_falloff(distance2, light.radius);
    light_dir = normalize(light_dir);

    const vec3 radiance = light.color * light.intensity * falloff;

    const vec3 halfway_dir = normalize(light_dir + shade_info.view);

    const float n_dot_h = clamp(dot(shade_info.normal, halfway_dir), 0.0f, 1.0f);
    const float n_dot_l = clamp(dot(shade_info.normal, light_dir), 0.0f, 1.0f);
    const float h_dot_l = clamp(dot(halfway_dir, light_dir), 0.0f, 1.0f);

    const float D = d_ggx(n_dot_h, shade_info.alpha_roughness);
    const float V = v_smith_correlated(shade_info.n_dot_v, n_dot_l, shade_info.alpha_roughness);
    const vec3 F = fresnel_schlick(h_dot_l, shade_info.F0, shade_info.F90);

    const vec3 diffuse = (vec3(1.0f) - F) * shade_info.diffuse_color * PI_INV;
    const vec3 specular = D * V * F;

    return (specular + diffuse) * radiance * n_dot_l;
}

vec3 shade_pbr_point_lights_clustered(ShadeInfo shade_info) {
    vec3 Lo = vec3(0.0f);
    const uint slice = slice_index_depth_linear(
        shade_info.z_view,
        u_camera.camera.near, u_camera.camera.far);
    const uint cluster_index = get_cluster_index(vertex_uv, slice);
    const Cluster cluster = u_clusters.clusters[cluster_index];
    for (uint bin = 0; bin < BIN_COUNT; bin++) {
        const uint mask_local = cluster.bins[bin];
        uint mask_group = subgroupOr(mask_local);
        while (mask_group > 0) {
            const uint bit_index = findLSB(mask_group);
            const uint light_index = bin * BIN_BIT_SIZE + bit_index;
            Lo += shade_pbr_point_light(shade_info, light_index);
            mask_group = mask_group & ~(1u << bit_index);
        }
    }
    
    return Lo;
}

vec3 shade_pbr_point_lights_tiled(ShadeInfo shade_info) {
    vec3 Lo = vec3(0.0f);
    const uint zbin_index = get_zbin_index(shade_info.depth,
    u_camera.camera.near, u_camera.camera.far);
    const uint tile_index = get_tile_index(vertex_uv, u_camera.camera.resolution);
    const Tile tile = u_tiles.tiles[tile_index];

    const uint light_min = uint(u_zbins.bins[zbin_index].min);
    const uint light_max = uint(u_zbins.bins[zbin_index].max);
    const uint light_min_group = subgroupMin(light_min);
    const uint light_max_group = subgroupMax(light_max);

    const uint bin_min = light_min_group / BIN_BIT_SIZE;
    const uint bin_max = light_max_group / BIN_BIT_SIZE;

    for (uint bin = bin_min; bin <= bin_max; bin++) {
        uint bin_bits = tile.bins[bin];
        const uint local_bit_min = clamp(int(light_min) - int(bin * 32), 0, int(BIN_BIT_SIZE - 1));
        const uint local_bit_width = clamp(int(light_max) - int(light_min) + 1, 0, int(BIN_BIT_SIZE));
        uint mask_local = ~0u;
        if (local_bit_width != BIN_BIT_SIZE) {
            mask_local = mask_local >> (BIN_BIT_SIZE - local_bit_width);
            mask_local = mask_local << local_bit_min;
        }

        bin_bits = bin_bits & mask_local;
        uint mask_group = subgroupOr(bin_bits);
        while(mask_group > 0) {
            const uint bit_index = findLSB(mask_group);
            const uint light_index = bin * BIN_BIT_SIZE + bit_index;
            Lo += shade_pbr_point_light(shade_info, light_index);
            mask_group = mask_group & ~(1u << bit_index);
        }
    }
    
    return Lo;
}

vec3 shade_pbr_point_lights_hybrid(ShadeInfo shade_info) {
    vec3 Lo = vec3(0.0f);
    const uint zbin_index = get_zbin_index(shade_info.depth,
    u_camera.camera.near, u_camera.camera.far);
    const uint tile_index = get_tile_index(vertex_uv, u_camera.camera.resolution);
    const Tile tile = u_tiles.tiles[tile_index];

    const uint slice = slice_index_depth_linear(
        shade_info.z_view,
        u_camera.camera.near, u_camera.camera.far);
    const uint cluster_index = get_cluster_index(vertex_uv, slice);
    const Cluster cluster = u_clusters.clusters[cluster_index];

    const uint light_min = uint(u_zbins.bins[zbin_index].min);
    const uint light_max = uint(u_zbins.bins[zbin_index].max);
    const uint light_min_group = subgroupMin(light_min);
    const uint light_max_group = subgroupMax(light_max);

    const uint bin_min = light_min_group / BIN_BIT_SIZE;
    const uint bin_max = light_max_group / BIN_BIT_SIZE;

    for (uint bin = bin_min; bin <= bin_max; bin++) {
        uint light_count;
        uint mask_group;
        {
            uint bin_bits = tile.bins[bin];
            const uint local_bit_min = clamp(int(light_min) - int(bin * 32), 0, int(BIN_BIT_SIZE - 1));
            const uint local_bit_width = clamp(int(light_max) - int(light_min) + 1, 0, int(BIN_BIT_SIZE));
            uint mask_local = ~0u;
            if (local_bit_width != BIN_BIT_SIZE) {
                mask_local = mask_local >> (BIN_BIT_SIZE - local_bit_width);
                mask_local = mask_local << local_bit_min;
            }

            bin_bits = bin_bits & mask_local;
            const uint mask_group_tiled = subgroupOr(bin_bits);
            light_count = bitCount(mask_group_tiled);
            mask_group = mask_group_tiled;
        }
        {
            const uint mask_local = cluster.bins[bin];
            const uint mask_group_clustered = subgroupOr(mask_local);
            if (light_count > bitCount(mask_group_clustered))
                mask_group = mask_group_clustered;
        }
        while(mask_group > 0) {
            const uint bit_index = findLSB(mask_group);
            const uint light_index = bin * BIN_BIT_SIZE + bit_index;
            Lo += shade_pbr_point_light(shade_info, light_index);
            mask_group = mask_group & ~(1u << bit_index);
        }
    }
    
    return Lo;
}

vec3 shade_pbr_directional_light(ShadeInfo shade_info, float directional_shadow) {
    const vec3 light_dir = -u_directional_light.light.direction;
    const vec3 radiance = u_directional_light.light.color * u_directional_light.light.intensity;

    const vec3 halfway_dir = normalize(light_dir + shade_info.view);

    const float n_dot_h = clamp(dot(shade_info.normal, halfway_dir), 0.0f, 1.0f);
    const float n_dot_l = clamp(dot(shade_info.normal, light_dir), 0.0f, 1.0f);
    const float h_dot_l = clamp(dot(halfway_dir, light_dir), 0.0f, 1.0f);

    const float D = d_ggx(n_dot_h, shade_info.alpha_roughness);
    const float V = v_smith_correlated(shade_info.n_dot_v, n_dot_l, shade_info.alpha_roughness);
    const vec3 F = fresnel_schlick(h_dot_l, shade_info.F0, shade_info.F90);

    const vec3 diffuse = (vec3(1.0f) - F) * shade_info.diffuse_color * PI_INV;
    const vec3 specular = D * V * F;
    
    return (specular + diffuse) * radiance * n_dot_l * (1.0f - directional_shadow);
}

vec3 shade_pbr_ibl(ShadeInfo shade_info) {
    const vec3 R = reflect(-shade_info.view, shade_info.normal);
    const vec3 irradiance = SH_irradiance_shade(u_irradiance_SH.sh, shade_info.normal).rgb;

    const float lod = shade_info.perceptual_roughness * MAX_REFLECTION_LOD;
    const vec3 prefiltered = textureLod(samplerCube(u_prefilter_map, u_sampler), R, lod).rgb;
    const vec2 brdf = textureLod(sampler2D(u_brdf, u_sampler_brdf),
    vec2(shade_info.n_dot_v, shade_info.perceptual_roughness), 0).rg;

    const vec3 diffuse = irradiance * shade_info.diffuse_color;
    const vec3 specular = (shade_info.F0 * brdf.x + shade_info.F90 * brdf.y) * prefiltered;

    const vec3 ambient = specular + diffuse;

    return ambient;
}