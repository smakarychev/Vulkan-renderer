vec3 uncharted_2_tonemap(vec3 color) {
	float A = 0.15f;
	float B = 0.50f;
	float C = 0.10f;
	float D = 0.20f;
	float E = 0.02f;
	float F = 0.30f;
	float W = 11.2f;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

vec3 tonemap(vec3 color, float exposure) {
    vec3 out_color = uncharted_2_tonemap(color * exposure);
    out_color = out_color * (1.0f / uncharted_2_tonemap(vec3(11.2f)));
    
    return out_color;
}