{
    "name": "scene-vbuffer-pbr-ugb",
    "rasterization":
    {
        "colors": [
            "RGBA16_FLOAT"
        ]
    },
    "shader_stages": [
        "processed/render-graph/common/fullscreen-vert.stage",
        "processed/render-graph/scene/draw-vbuffer-pbr-frag.stage"
    ],
    "bindless": "main_materials" 
}