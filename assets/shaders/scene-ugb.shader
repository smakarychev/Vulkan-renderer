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
        "processed/render-graph/scene/draw-ugb-vert.stage",
        "processed/render-graph/scene/draw-ugb-frag.stage"
    ],
    "bindless": "main_materials" 
}