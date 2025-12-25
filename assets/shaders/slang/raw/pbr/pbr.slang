module pbr;

__include "shadingInfo";
__include "pbrShading";

import "core/lib";

/// Returns GGX sampled point on a unit sphere:
/// @param uv polar and azimuthal angles
/// @param normal hemisphere normal vector
/// @param roughness surface roughness
/// @return GGX point on a unit sphere
public float3 importanceGgxHemisphereSample(float2 uv, float3 normal, float roughness) {
    const float a = roughness * roughness;
        
    const float u = uv.x;
    const float v = uv.y;
    
    const float phi = u * TAU;
    float cosTheta = sqrt((1.0f - v) / (1.0f + (a * a - 1.0f) * v));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
    
    const float3 local = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    
    const float3 up = abs(normal.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    const float3 tangent = normalize(cross(up, normal));
    const float3 bitangent = normalize(cross(normal, tangent));
    
    return normalize(local.x * tangent + local.y * bitangent + local.z * normal);
}

/// Returns GGX normal distribution function
/// @param NoH cosine between normal and halfvector
/// @param roughness surface roughness
/// @return GGX normal distribution function value
public float ggxNormalDistribution(float NoH, float roughness) {
    const float a2 = roughness * roughness;
    const float f = (NoH * a2 - NoH) * NoH + 1.0f;
    
    return a2 / (PI * f * f);
}

/// Returns GGX Smith height-correlated shadow-masking function
/// @param NoV cosine between normal and view vector
/// @param NoL cosine between normal and light vector
/// @param roughness surface roughness
/// @return GGX Smith height-correlated shadow-masking function value
public float ggxGSmithHeightCorrelated(float NoV, float NoL, float roughness) {
    const float a2 = roughness * roughness;
    const float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    const float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);

    return 2.0f * NoL * NoV / (GGXV + GGXL);
}

/// Returns GGX Smith height-correlated visibility function
/// @param NoV cosine between normal and view vector
/// @param NoL cosine between normal and light vector
/// @param roughness surface roughness
/// @return GGX Smith height-correlated visibility function value
public float ggxVSmithHeightCorrelated(float NoV, float NoL, float roughness) {
    const float a2 = roughness * roughness;
    const float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    const float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);

    return 0.5f / (GGXV + GGXL);
}

/// Returns Shlick approximation of Fresnel function
/// @param theta cosine of the angle between the incoming and outgoing light directions
/// @param F0 specular reflectance at normal incidence
/// @param F90 specular reflectance at grazing angles
/// @return Shlick approximation of Fresnel function
public float3 fresnelSchlick(float theta, float3 f0, float3 f90) {
    return f0 + (f90 - f0) * pow(clamp(1.0f - theta, 0.0f, 1.0f), 5.0f);
}

/// Returns renormalized Disney diffuse brdf, see "moving frostbite to pbr" paper
/// @param NoV cosine between normal and view vector
/// @param NoL cosine between normal and light vector
/// @param NoH cosine between normal and halfvector
/// @param linearRoughness linear surface roughness
/// @return Renormalized Disney diffuse brdf value
public float disneyDiffuse(float NoV, float NoL, float NoH, float linearRoughness) {
    const float energyBias = lerp(0.0f, 0.5f, linearRoughness);
    const float energyFactor = lerp(1.0f, 1.0f / 1.51f, linearRoughness);
    const float fd90 = energyBias + 2.0f * NoH * NoH * linearRoughness;
    const float3 f0 = float3(1.0f);
    const float lightScatter = fresnelSchlick(NoL, f0, fd90).r;
    const float viewScatter = fresnelSchlick(NoV, f0, fd90).r;

    return lightScatter * viewScatter * energyFactor;
}

/// Returns falloff in range of 0 to 1 for a point light
/// @param distance2 squared distance between a shaded point and point light
/// @param radius raduis of a point light
/// @return Inverted falloff amount
internal float pointLightFalloff(float distance2, float radius) {
    const float distance2OverRadius2 = distance2 / (radius * radius);
    const float nom = saturate(1.0f - distance2OverRadius2 * distance2OverRadius2);
    
    return nom * nom / (distance2 + 1.0f);
}