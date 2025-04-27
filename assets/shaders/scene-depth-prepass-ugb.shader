{
    "name": "scene-depth-prepass-ugb",
    "rasterization":
    {
        "depth": "D32_FLOAT", 
        "alpha_blending": "none"
    },
    "shader_stages": [
        "processed/render-graph/scene/draw-depth-ugb-vert.stage",
        "processed/render-graph/scene/draw-depth-ugb-frag.stage"
    ],
    "bindless": "main_materials" 
}