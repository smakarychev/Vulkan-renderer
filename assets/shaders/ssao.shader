{
    "name": "ssao",
    "shader_stages": [
        "../assets/shaders/processed/render-graph/ao/ssao-comp.stage"
    ],
    "specialization_constants": [
        {
		    "name": "MAX_SAMPLES",
		    "value": 128,
		    "type": "u32" 
	    }
    ]
}