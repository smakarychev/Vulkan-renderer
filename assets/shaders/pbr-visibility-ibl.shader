{
    "name": "pbr-visibility-ibl",
    "rasterization":
    {
        "colors": [
            "RGBA16_FLOAT"
        ]
    },
    "shader_stages": [
        "../assets/shaders/processed/render-graph/common/fullscreen-vert.stage",
        "../assets/shaders/processed/render-graph/pbr/pbr-visibility-buffer-ibl-frag.stage"
    ],
    "bindless": "main_materials" 
}