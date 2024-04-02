uvec2 unflatten2d(uint index, uint size) {
    return uvec2(index % size, index / size); 
}

uint flatten2d(uvec2 coords, uint size) {
    return coords.x + coords.y * size;
}

float linear_depth(float depth, float near, float far) {
    float z_n = 2.0f * depth - 1.0f;
    float lin = 2.0f * far * near / (near + far - z_n * (near - far));
    
    return lin;
}

vec3 reconstruct_position(vec2 uv, float z, mat4 inverse_view_projection) {
	float x = uv.x * 2.0f - 1.0f;
	float y = (1.0f - uv.y) * 2.0f - 1.0f;
	vec4 position = inverse_view_projection * vec4(x, y, z, 1.0f);
	return vec3(position.xyz / position.w);
}

bool is_saturated(float val) {
    return val == clamp(val, 0.0f, 1.0f);
}

bool is_saturated(vec2 val) {
    return is_saturated(val.x) && is_saturated(val.y);
}

bool is_saturated(vec3 val) {
    return is_saturated(val.xy) && is_saturated(val.z);
}