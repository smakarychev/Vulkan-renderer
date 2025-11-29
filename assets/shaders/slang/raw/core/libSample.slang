implementing lib;

/// Returns uv of Hammersley point on a sphere:
///
/// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
/// @param i ith point to be generated
/// @param n total number of points to be generated
/// @return `uv` polar and azimuthal angles
public float2 hammersley2d(uint i, uint n) {
    uint radicalInverseI = i;
    radicalInverseI = (radicalInverseI << 16u) | (radicalInverseI >> 16u);
    radicalInverseI = ((radicalInverseI & 0x55555555u) << 1u) | ((radicalInverseI & 0xAAAAAAAAu) >> 1u);
    radicalInverseI = ((radicalInverseI & 0x33333333u) << 2u) | ((radicalInverseI & 0xCCCCCCCCu) >> 2u);
    radicalInverseI = ((radicalInverseI & 0x0F0F0F0Fu) << 4u) | ((radicalInverseI & 0xF0F0F0F0u) >> 4u);
    radicalInverseI = ((radicalInverseI & 0x00FF00FFu) << 8u) | ((radicalInverseI & 0xFF00FF00u) >> 8u);

    const float radicalInverse = float(radicalInverseI) * 2.3283064365386963e-10;
    
    return float2(float(i) / float(n), radicalInverse);
}

/// Returns uniform point on a unit sphere:
/// @param uv polar and azimuthal angles
/// @return Uniform point on a unit sphere
public float3 uniformSphereSample(float2 uv) {
    const float phi = TAU * uv.x;
    const float cosTheta = 1.0f - 2.0f * uv.y;
    const float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    return float3(sinTheta * cos(phi), cosTheta, sinTheta * sin(phi));
}

/// Returns local uniform point on a unit hemisphere:
/// @param uv polar and azimuthal angles
/// @return Local uniform point on a unit hemisphere (`z` is up)
public float3 uniformHemisphereSample(float2 uv) {
    const float phi = TAU * uv.x;
    const float cosTheta = 1.0f - uv.y;
    const float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    return float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

/// Returns local cosine-distributed point on a unit hemisphere:
/// @param uv polar and azimuthal angles
/// @return Local cosine-distributed point on a unit hemisphere (`z` is up)
public float3 cosineHemisphereSample(float2 uv) {
    const float phi = TAU * uv.x;
    const float cosTheta = sqrt(1.0f - uv.y);
    const float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    return float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

/// Returns uniform point on a unit hemisphere:
/// @param uv polar and azimuthal angles
/// @param normal hemisphere normal vector
/// @return uniform point on a unit hemisphere
public float3 uniformHemisphereSample(float2 uv, float3 normal) {
    const float3 up = abs(normal.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    const float3 tangent = normalize(cross(up, normal));
    const float3 bitangent = normalize(cross(normal, tangent));

    const float3 local = uniformHemisphereSample(uv);

    return normalize(local.x * tangent + local.y * bitangent + local.z * normal);
}