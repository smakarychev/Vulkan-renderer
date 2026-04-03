module ugb;

import "geometry";

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

/// Represents Unified Geometry Buffer
public struct UGBMeshlets {
    ByteAddressBuffer meshletsUgb;

    /// Read meshlet bounds data from ugb at a given index
    /// @param index element index of meshlet bounds data
    public MeshletBounds bounds(uint index) {
        return loadAtIndex<MeshletBounds>(index);
    }
    
    /// Read meshlets data from ugb at a given index
    /// @param index element index of meshlet data
    public Meshlet meshlet(uint index) {
        return loadAtIndex<Meshlet>(index);
    }

    private ValueT loadAtIndex<ValueT>(uint index) {
        return meshletsUgb.LoadAligned<ValueT>(index * sizeof(ValueT), sizeof(ValueT));
    }
}