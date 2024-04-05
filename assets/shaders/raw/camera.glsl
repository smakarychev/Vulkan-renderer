struct CameraGPU {
    mat4 view_projection;
    mat4 projection;
    mat4 view;

    vec3 position;
    float near;
    vec3 forward;
    float far;

    mat4 inv_view_projection;
    mat4 inv_projection;
    mat4 inv_view;
    
    vec2 resolution;
};