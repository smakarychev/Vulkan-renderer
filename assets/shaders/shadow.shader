{
    "name": "shadow",
    "rasterization":
    {
        "depth": "D32_FLOAT", 
        "alpha_blending": "none",
        "depth_clamp": true
    },
    "dynamic_states": [
        "depth_bias"
    ],
    "shader_stages": [
        "../assets/shaders/processed/render-graph/shadows/directional-vert.stage"
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