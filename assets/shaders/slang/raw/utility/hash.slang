module hash;

namespace hash {
internal float3 toColor(uint x) {
    return float3(
        float(x & 255u) / 255.0f,
        float((x >> 8) & 255u) / 255.0f,
        float((x >> 16) & 255u) / 255.0f);
}

public uint hash(uint x) {
    const uint state = x * 747796405u + 2891336453u;
    const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;

    return (word >> 22u) ^ word;
}

public float3 color(uint x) {
    return toColor(hash(x));
}
}