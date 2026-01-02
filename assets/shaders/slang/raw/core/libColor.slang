implementing lib;

namespace color {

public float3 hash(uint val) {
    return ::toColor(::hash(val));
}

public float3 heatmap(float t) {
    t = t * 3.1415 * 0.5f;
    
    return float3(sin(t), sin(2.0f * t), cos(t));
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