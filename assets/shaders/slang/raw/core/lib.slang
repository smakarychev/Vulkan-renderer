module lib;

public static float PI = 3.14159265359f;
public static float TAU = PI * 2.0f;;

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

/// Get cubemap normal vector
/// @param coord `x` and `y` components are 2d pixel coordinates within the face. `z` component is the face index
/// @param inverseFaceSize inverse size of a 2d cubemap face
/// @return The normalized cubemap face normal vector
public float3 getCubemapNormal(uint3 coord, float2 inverseFaceSize) { 
    const float2 st = float2(coord.xy + 0.5f) * inverseFaceSize;
    const float2 uv = 2.0f * float2(st.x, 1.0f - st.y) - float2(1.0f);

    float3 ret;
    if (coord.z == 0)
        ret = float3(1.0,  uv.y, -uv.x);
    else if (coord.z == 1)
        ret = float3(-1.0, uv.y,  uv.x);
    else if (coord.z == 2)
        ret = float3(uv.x, 1.0, -uv.y);
    else if (coord.z == 3)
        ret = float3(uv.x, -1.0, uv.y);
    else if (coord.z == 4)
        ret = float3(uv.x, uv.y, 1.0);
    else if (coord.z == 5)
        ret = float3(-uv.x, uv.y, -1.0);
    
    return normalize(ret);
}
