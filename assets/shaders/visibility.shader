{
    "name": "visibility",
    "rasterization":
    {
        "colors": [
            "R32_UINT"
        ],
        "depth": "D32_FLOAT", 
        "alpha_blending": "none"
    },
    "shader_stages": [
        "../assets/shaders/processed/render-graph/general/visibility-buffer-vert.stage",
        "../assets/shaders/processed/render-graph/general/visibility-buffer-frag.stage"
    ],
    "bindless": "main_materials" 
}