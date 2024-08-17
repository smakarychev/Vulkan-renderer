{
    "name": "light-clusters-compact",
    "shader_stages": [
        "../assets/shaders/processed/render-graph/lights/compact-active-clusters-comp.stage"
    ],
    "specialization_constants": [
        {
            "name": "IDENTIFY",
            "value": false,
            "type": "b32"
        },
        {
            "name": "COMPACT",
            "value": false,
            "type": "b32"
        }
    ]
}