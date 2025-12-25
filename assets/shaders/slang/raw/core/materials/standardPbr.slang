implementing material;

import "core/attributes";

[StandaloneType("standardPbrMaterial")]
public struct StandardPbrMaterial {
    public float4 albedo;
    public float metallic;
    public float roughness;
    public uint albedoTexture;
    public uint normalTexture;
    public uint metallicRoughnessTexture;
    public uint ambientOcclusionTexture;
    public uint emissiveTexture;
};