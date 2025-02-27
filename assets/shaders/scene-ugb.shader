{
    "name": "scene-ugb",
    "rasterization":
    {
        "colors": [
            "RGBA16_FLOAT"
        ],
        "depth": "D32_FLOAT"
    },
    "shader_stages": [
        "processed/render-graph/general/draw-ugb-vert.stage",
        "processed/render-graph/general/draw-ugb-frag.stage"
    ],
    "bindless": "main_materials" 
}