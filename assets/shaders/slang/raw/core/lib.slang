module lib;

/// Check if value is contained within [0, 1]
/// @param val value to be checked
/// @return `true` if value `val` is within [0, 1], `false` otherwise
bool isSaturated(float val) {
    return val == saturate(val);
}

/// Check if each coordinate of a vector is contained within [0, 1]
/// @param val value to be checked
/// @return `true` if each coordinate of a vector `val` is within [0, 1], `false` otherwise
public bool isSaturated(float2 val) {
    return isSaturated(val.x) && isSaturated(val.y);
}

/// Check if each coordinate of a vector is contained within [0, 1]
/// @param val value to be checked
/// @return `true` if each coordinate of a vector `val` is within [0, 1], `false` otherwise
public bool isSaturated(float3 val) {
    return isSaturated(val.xy) && isSaturated(val.z);
}

/// Reconstructs position from uv coordinates and depth-buffer-z
/// @param uv uv coordinates of a point
/// @param z value sampled from the depth buffer
/// @param inverseViewProjection inverse view-projection matrix
/// @return The reconstructed world-space position of a point
public float3 reconstructPosition(float2 uv, float z, float4x4 inverseViewProjection) {
	float x = uv.x * 2.0f - 1.0f;
	float y = (1.0f - uv.y) * 2.0f - 1.0f;
	float4 position = mul(float4(x, y, z, 1.0f), inverseViewProjection);

	return float3(position.xyz / position.w);
}
