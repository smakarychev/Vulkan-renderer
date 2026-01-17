implementing material;

import "core/attributes";
import "core/texturesHeap";

[StandaloneType("standardPbrMaterial")]
public struct StandardPbrMaterial {
    public float4 albedo;
    public float metallic;
    public float roughness;
    public TextureHandle<Texture2D> albedoTexture;
    public TextureHandle<Texture2D> normalTexture;
    public TextureHandle<Texture2D> metallicRoughnessTexture;
    public TextureHandle<Texture2D> ambientOcclusionTexture;
    public TextureHandle<Texture2D> emissiveTexture;
};