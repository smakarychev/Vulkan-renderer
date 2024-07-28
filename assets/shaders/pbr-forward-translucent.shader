{
    "name": "pbr-forward-translucent",
    "rasterization":
    {
        "colors": [
            "RGBA16_FLOAT"
        ],
        "depth": "D32_FLOAT",
        "depth_mode": "read",
        "depth_clamp": false,
        "cull_mode": "none"
    },
    "shader_stages": [
        "../assets/shaders/processed/render-graph/pbr/pbr-translucency-vert.stage",
        "../assets/shaders/processed/render-graph/pbr/pbr-translucency-frag.stage"
    ],
    "specialization_constants": [
        {
		    "name": "COMPOUND_INDEX",
		    "value": false,
		    "type": "b32" 
	    },
        {
            "name": "MAX_REFLECTION_LOD",
            "value": "5.0",
            "type": "f32"
        }
    ],
    "bindless": "main_materials" 
}