{
    "name": "scene-vbuffer-ugb",
    "rasterization":
    {
        "colors": [
            "R32_UINT"
        ],
        "depth": "D32_FLOAT", 
        "alpha_blending": "none"
    },
    "shader_stages": [
        "processed/render-graph/general/vbuffer-ugb-vert.stage",
        "processed/render-graph/general/vbuffer-ugb-frag.stage"
    ],
    "bindless": "main_materials" 
}