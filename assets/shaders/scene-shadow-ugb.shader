{
    "name": "scene-shadow-ugb",
    "rasterization":
    {
        "depth": "D32_FLOAT",
        "alpha_blending": "none",
        "depth_clamp": true
    },
    "dynamic_states": [
        "depth_bias"
    ],
    "shader_stages": [
        "processed/render-graph/shadows/directional-ugb-vert.stage"
    ],
    "bindless": "main_materials"
}