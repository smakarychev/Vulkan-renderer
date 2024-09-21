{
    "name": "pbr-visibility-ibl",
    "rasterization":
    {
        "colors": [
            "RGBA16_FLOAT"
        ]
    },
    "shader_stages": [
        "../assets/shaders/processed/render-graph/common/fullscreen-vert.stage",
        "../assets/shaders/processed/render-graph/pbr/pbr-visibility-buffer-ibl-frag.stage"
    ],
    "specialization_constants": [
        {
            "name": "MAX_REFLECTION_LOD",
            "value": 5.0,
            "type": "f32"
        },
        {
            "name": "USE_TILED_LIGHTING",
            "value": false,
            "type": "b32"
        },
        {
            "name": "USE_CLUSTERED_LIGHTING",
            "value": false,
            "type": "b32"
        },
        {
            "name": "USE_HYBRID_LIGHTING",
            "value": false,
            "type": "b32"
        }
    ],
    "bindless": "main_materials" 
}