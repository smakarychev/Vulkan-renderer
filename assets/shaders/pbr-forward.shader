{
    "name": "pbr-forward",
    "rasterization":
    {
        "colors": [
            "RGBA16_FLOAT"
        ],
        "depth": "D32_FLOAT"
    },
    "shader_stages": [
        "processed/render-graph/pbr/pbr-ibl-vert.stage",
        "processed/render-graph/pbr/pbr-ibl-frag.stage"
    ],
    "bindless": "main_materials" 
}