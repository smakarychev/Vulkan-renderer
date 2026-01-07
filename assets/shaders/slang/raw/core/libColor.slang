implementing lib;

namespace color {

public float3 hash(uint val) {
    return ::toColor(::hash(val));
}

public float3 heatmap(float t) {
    t = t * 3.1415 * 0.5f;
    
    return float3(sin(t), sin(2.0f * t), cos(t));
}

public float3 tonemap(float3 color, float exposure) {
    float3 outColor = uncharted2Tonemap(color * exposure);
    outColor = outColor * (1.0f / uncharted2Tonemap(float3(11.2f)));
    
    return outColor;
}

}

internal float3 toColor(uint x) {
    return float3(
        float(x & 255u) / 255.0f,
        float((x >> 8) & 255u) / 255.0f,
        float((x >> 16) & 255u) / 255.0f);
}

internal uint hash(uint x) {
    const uint state = x * 747796405u + 2891336453u;
    const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;

    return (word >> 22u) ^ word;
}

internal float3 uncharted2Tonemap(float3 color) {
	float A = 0.15f;
	float B = 0.50f;
	float C = 0.10f;
	float D = 0.20f;
	float E = 0.02f;
	float F = 0.30f;
	float W = 11.2f;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}