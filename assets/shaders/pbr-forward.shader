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
        "../assets/shaders/processed/render-graph/pbr/pbr-ibl-vert.stage",
        "../assets/shaders/processed/render-graph/pbr/pbr-ibl-frag.stage"
    ],
    "bindless": "main_materials" 
}