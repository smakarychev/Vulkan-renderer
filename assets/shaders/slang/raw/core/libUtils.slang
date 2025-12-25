implementing lib;

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

/// Get 2d index from 1d index of a 2d grid cell
public uint2 unflatten2d(uint index, uint2 size) {
    return uint2(index % size.x, index / size.y); 
}

/// Get 1d index for a cell of a 2d grid
public uint flatten2d(uint2 coords, uint2 size) {
    return coords.x + coords.y * size.x;
}