module ugb;

/// Represents Unified Geometry Buffer
public struct UGB {
    ByteAddressBuffer ugb;

    /// Read position data from ugb at a given index
    /// @param index element index of vertex position data
    public float3 position(uint index) {
        return loadAtIndex<float3>(index);
    }
    
    /// Read normal data from ugb at a given index
    /// @param index element index of vertex normal data
    public float3 normal(uint index) {
        return loadAtIndex<float3>(index);
    }
    
    /// Read tangent data from ugb at a given index
    /// @param index element index of vertex tangent data
    public float4 tangent(uint index) {
        return loadAtIndex<float4>(index);
    }
    
    /// Read uv data from ugb at a given index
    /// @param index element index of vertex uv data
    public float2 uv(uint index) {
        return loadAtIndex<float2>(index);
    }

    private ValueT loadAtIndex<ValueT>(uint index) {
        return ugb.LoadAligned<ValueT>(index * sizeof(ValueT), sizeof(ValueT));
    }
}