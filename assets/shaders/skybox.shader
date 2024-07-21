{
    "name": "skybox",
    "rasterization":
    {
        "colors": [
            "RGBA16_FLOAT"
        ],
        "depth": "D32_FLOAT" 
    },
    "shader_stages": [
        "../assets/shaders/processed/render-graph/general/skybox-vert.stage",
        "../assets/shaders/processed/render-graph/general/skybox-frag.stage"
    ],
    "bindless": "main_materials" 
}