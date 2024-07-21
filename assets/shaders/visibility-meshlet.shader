{
    "name": "visibility-meshlet",
    "rasterization":
    {
        "colors": [
            "R32_UINT"
        ],
        "depth": "D32_FLOAT", 
        "alpha_blending": "none"
    },
    "shader_stages": [
        "../assets/shaders/processed/render-graph/general/visibility-buffer-vert.stage",
        "../assets/shaders/processed/render-graph/general/visibility-buffer-frag.stage"
    ],
    "specialization_constants": [
        {
		    "name": "COMPOUND_INDEX",
		    "value": false,
		    "type": "b32" 
	    }
    ],
    "bindless": "main_materials" 
}