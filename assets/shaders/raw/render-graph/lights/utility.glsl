vec3 convert_to_view(vec4 screen, mat4 projection_inverse) {
    const vec2 ndc = screen.xy / u_render_size;
    vec4 clip = vec4((ndc - 0.5f) * 2.0f, screen.z, screen.w);
    clip.y = -clip.y;
    const vec4 view = projection_inverse * clip;

    return view.xyz / view.w;
}