{
    "name": "triangle-cull-multiview",
    "shader_stages": [
        "../assets/shaders/processed/render-graph/culling/multiview/triangle-cull-comp.stage"
    ],
    "specialization_constants": [
        {
		    "name": "REOCCLUSION",
		    "value": false,
		    "type": "b32" 
	    },
        {
          "name": "SINGLE_PASS",
          "value": false,
          "type": "b32"
        }
    ]
}