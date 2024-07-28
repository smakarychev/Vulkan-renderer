{
    "name": "mesh-cull-multiview",
    "shader_stages": [
        "../assets/shaders/processed/render-graph/culling/multiview/mesh-cull-comp.stage"
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