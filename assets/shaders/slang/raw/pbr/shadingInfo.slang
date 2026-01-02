implementing pbr;

import "core/viewInfo";

public struct PbrShadingInfo {
    /// Position of the shaded pixel in world space
    public float3 position;
    /// Normal of the shaded pixel in world space
    public float3 normal;
    /// Z position of the shaded pixel in view space
    public float viewZ;
    /// Depth of the shaded pixel after perspective transform
    public float depth;
    /// View vector from position of shaded pixel in world space to camera position in world space
    public float3 view;
    /// Cosine between normal and view vector
    public float NoV;
    /// Roughness 
    /// Note that roughness is *perceptially linear* but it is **not** *linear roughness*
    /// it is remapped to a square of material roughness
    public float roughness;
    /// Metallic that comes from material description
    public float metallic;   
    /// Specular reflectance at normal incidence          
    public float3 F0;
    /// Specular reflectance at grazing angles                 
    public float3 F90;
    /// Material diffuse color according to its metallic properties
    public float3 diffuseColor;
    /// Material specular color
    public float3 specularColor;
    /// Screen pixel uv coorinates
    public float2 screenUv;
    /// Shadow value for shaded point
    public float directionalShadow;

    public static float3 calculateF0(float4 albedo, float metallic) {
        const float reflectance = 0.5f;
        return lerp(float3(0.16f * reflectance * reflectance), albedo.rgb, metallic);
    }
    public static float3 calculateF90() {
        return float3(1.0f);
    }
    public static float3 calculateDiffuseColor(float4 albedo, float metallic, float3 F0) {
        return (1.0f - metallic) * albedo.rgb * (float3(1.0f) - F0);
    }
    public static float3 calculateSpecularColor(float3 F0) {
        return F0;
    }
}