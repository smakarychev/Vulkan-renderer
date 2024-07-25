{
    "name": "ssao-blur",
    "shader_stages": [
        "../assets/shaders/processed/render-graph/ao/ssao-blur-comp.stage"
    ],
    "specialization_constants": [
        {
		    "name": "IS_VERTICAL",
		    "value": true,
		    "type": "b32" 
	    }
    ]
}