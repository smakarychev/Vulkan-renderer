{
    "name": "materials",
    "rasterization":
    {
        "colors": [
            "RGBA16_FLOAT"
        ]
    },
    "shader_stages": [
        "processed/render-graph/common/fullscreen-vert.stage",
        "processed/core/material-frag.stage",
        "processed/core/material-comp.stage"
    ],
    "bindless_count": 1024
}