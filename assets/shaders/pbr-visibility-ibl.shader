{
    "name": "pbr-visibility-ibl",
    "rasterization":
    {
        "colors": [
            "RGBA16_FLOAT"
        ]
    },
    "shader_stages": [
        "processed/render-graph/common/fullscreen-vert.stage",
        "processed/render-graph/pbr/pbr-visibility-buffer-ibl-frag.stage"
    ],
    "bindless": "main_materials" 
}