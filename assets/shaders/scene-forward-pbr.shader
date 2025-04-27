{
    "name": "scene-forward-pbr",
    "rasterization":
    {
        "colors": [
            "RGBA16_FLOAT"
        ],
        "depth": "D32_FLOAT"
    },
    "shader_stages": [
        "processed/render-graph/scene/draw-forward-pbr-vert.stage",
        "processed/render-graph/scene/draw-forward-pbr-frag.stage"
    ],
    "bindless": "main_materials" 
}