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