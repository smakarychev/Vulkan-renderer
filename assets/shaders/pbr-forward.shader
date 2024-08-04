{
    "name": "pbr-forward",
    "rasterization":
    {
        "colors": [
            "RGBA16_FLOAT"
        ],
        "depth": "D32_FLOAT"
    },
    "shader_stages": [
        "../assets/shaders/processed/render-graph/pbr/pbr-ibl-vert.stage",
        "../assets/shaders/processed/render-graph/pbr/pbr-ibl-frag.stage"
    ],
    "specialization_constants": [
        {
		    "name": "COMPOUND_INDEX",
		    "value": false,
		    "type": "b32" 
	    },
        {
            "name": "MAX_REFLECTION_LOD",
            "value": 5.0,
            "type": "f32"
        }
    ],
    "bindless": "main_materials" 
}