{
    "name": "pbr-forward-translucent",
    "rasterization":
    {
        "colors": [
            "RGBA16_FLOAT"
        ],
        "depth": "D32_FLOAT",
        "depth_mode": "read",
        "depth_clamp": false,
        "cull_mode": "none"
    },
    "shader_stages": [
        "processed/render-graph/pbr/pbr-translucency-vert.stage",
        "processed/render-graph/pbr/pbr-translucency-frag.stage"
    ],
    "bindless": "main_materials" 
}