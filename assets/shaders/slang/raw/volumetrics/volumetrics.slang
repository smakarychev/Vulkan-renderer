module volumetrics;

__include "integrateAtmosphereScatteredLumunance";
__include "atmosphereLuts";
__include "atmosphereRender";
__include "clouds/clouds";
__include "clouds/cloudsVPInfo";
__include "clouds/cloudsVP";

import "core/attributes";
import "core/lib";
import "core/viewInfo";

// based on "A Scalable and Production Ready Sky and Atmosphere Rendering Technique by Sébastien Hillaire (Epic Games, Inc)"
// https://github.com/sebh/UnrealEngineSkyAtmosphere

internal static const float TRANSMITTANCE_LUT_WIDTH = 256.0f;
internal static const float TRANSMITTANCE_LUT_HEIGHT = 64.0f;

internal static const float SKY_VIEW_LUT_WIDTH = 200.0f;
internal static const float SKY_VIEW_LUT_HEIGHT = 100.0f;

internal static const float MULTISCATTERING_LUT_RES = 32.0f;
internal static const float AERIAL_PERSPECTIVE_LUT_RES = 32.0f;

internal static const float AERIAL_MM_PER_SLICE = 4.0f;

internal static const float MAX_DEPTH = 3.402823466e+38f;

internal static const float TRANSMITTANCE_STEPS = 40.0f;
internal static const float SKY_STEPS = 30.0f;
internal static const float MULTISCATTERING_SPHERE_SAMPLES = 64.0f;
internal static const uint MULTISCATTERING_SPHERE_SAMPLES_INT = 64;
internal static const float MULTISCATTERING_STEPS = 20.0f;

internal static const float PLANET_RADIUS_OFFSET_UV = 0.01f;
internal static const float PLANET_RADIUS_OFFSET_KM = 0.01f;

internal static const float DEFAULT_VIEW_HEIGHT_M = 300.0f;

internal float3 getViewPosition(float3 cameraPos, float surfaceRadius) {
    const float baseViewHeight = DEFAULT_VIEW_HEIGHT_M * 1e-3f;
    
    return cameraPos * 1e-3f + float3(0.0f, surfaceRadius + baseViewHeight, 0.0f);
}

internal float3 getViewPositionMeters(float3 cameraPos, float surfaceRadiusMeters) {
    return cameraPos + float3(0.0f, surfaceRadiusMeters + DEFAULT_VIEW_HEIGHT_M, 0.0f);
}

internal float3 getWorldPosition(float3 viewPos, float surfaceRadius) {
    const float baseViewHeight = DEFAULT_VIEW_HEIGHT_M * 1e-3f;
    
    return (viewPos - float3(0.0f, surfaceRadius + baseViewHeight, 0.0f)) * 1e+3f;
}

/// Rayleigh phase value
/// @param cosh the angle between incident and outgoing scattering directions
/// @return Value of describing the distribution of light directions after a scattering event
public float rayleighPhase(float cosh) {
    return 3.0f / (16.0f * PI) * (1.0f + cosh * cosh);
}

/// Mie phase value approximation using Cornette-Shanks phase function
/// @param cosh the angle between incident and outgoing scattering directions
/// @param g asymmetry parameter
/// @return Value of describing the distribution of light directions after a scattering event
public float mieCSPhase(float cosh, float g = 0.8f) {
    const float scale = 3.0f / (8.0f * PI);
    const float nom = scale * (1.0f - g * g) * (1.0f + cosh * cosh);
    const float denom = (2.0f + g * g) * pow(1.0f + g * g - 2.0f * g * cosh, 1.5f);

    return nom / denom;
}

/// Henyey-Greenstein phase value
/// @param cosh the angle between incident and outgoing scattering directions
/// @param g asymmetry parameter
/// @return Value of describing the distribution of light directions after a scattering event
public float henyeyGreensteinPhase(float cosh, float g) {
    return (1.0f / (4.0f * PI)) *
        ((1.0f - g * g) / pow(1.0f + g * g - 2.0f * g * cosh, 1.5f));
}

/// Represents scattering properties of atmosphere media
public struct AtmosphereMedia {
    /// Get atmosphere scattering sample at point
    /// @param x sample point
    /// @param atmosphere atmosphere settings for a particular view
    public static AtmosphereMedia sampleAt(float3 x, const AtmosphereSettings atmosphere) {
        const float altitudeKm = length(x) - atmosphere.surface;
        const float rayleighDensity = exp(-altitudeKm / (8.0f * atmosphere.rayleighDensity));
        const float mieDensity = exp(-altitudeKm / (1.2f * atmosphere.mieDensity));

        AtmosphereMedia media;
        media.rayleigh = atmosphere.rayleighScattering.rgb * rayleighDensity;
        media.mie = atmosphere.mieScattering.rgb * mieDensity;
        const float3 rayleighAbsorption = atmosphere.rayleighAbsorption.rgb * rayleighDensity;
        const float3 mieAbsorption = atmosphere.mieAbsorption.rgb * mieDensity;

        const float3 ozoneAbsorption = atmosphere.ozoneAbsorption.rgb * 
            max(0.0f, 1.0f - abs(altitudeKm - 25.0f) / 15.0f);

        media.extinction =
            media.rayleigh + rayleighAbsorption +
            media.mie + mieAbsorption +
            ozoneAbsorption;

        return media;
    }

    public float3 rayleigh;
    public float3 mie;
    public float3 extinction;
}

// https://ebruneton.github.io/precomputed_atmospheric_scattering/atmosphere/functions.glsl.html
internal float distanceToAtmosphereTop(const AtmosphereSettings atmosphere, float r, float mu) {
    const float discriminant = r * r * (mu * mu - 1.0f) + atmosphere.atmosphere * atmosphere.atmosphere;

    return max(0.0f, -r * mu + sqrt(max(0.0, discriminant)));
}

internal float2 transmittanceUvFromRMu(const AtmosphereSettings atmosphere, float r, float mu) {
    const float H = sqrt(
        atmosphere.atmosphere * atmosphere.atmosphere -
        atmosphere.surface * atmosphere.surface);
    const float rho = sqrt(max(r * r - atmosphere.surface * atmosphere.surface, 0.0f));

    const float d = distanceToAtmosphereTop(atmosphere, r, mu);
    const float dMin = atmosphere.atmosphere - r;
    const float dMax = H + rho;

    return float2((d - dMin) / (dMax - dMin), rho / H);
}

internal float2 transmittanceRMuFromUv(const AtmosphereSettings atmosphere, float2 uv) {
    const float H = sqrt(
        atmosphere.atmosphere * atmosphere.atmosphere -
        atmosphere.surface * atmosphere.surface);
    const float rho = H * uv.y;
    const float r = sqrt(rho * rho + atmosphere.surface * atmosphere.surface);

    const float dMin = atmosphere.atmosphere - r;
    const float dMax = H + rho;
    const float d = dMin + uv.x * (dMax - dMin);
    const float mu = clamp(d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0f * r * d), -1.0f, 1.0f);

    return float2(r, mu);
}

internal float2 unitToSubUv(float2 uv, float2 res) {
    return float2(uv + 0.5f / res) * (res / (res + 1.0f));
}

internal float2 subUvToUnit(float2 uv, float2 res) {
    return float2(uv - 0.5f / res) * (res / (res - 1.0f));
}

internal float2 skyViewZenCosFromUv(const AtmosphereSettings atmosphere, float2 uv, float r) {
    uv = subUvToUnit(uv, float2(SKY_VIEW_LUT_WIDTH, SKY_VIEW_LUT_HEIGHT));

    const float rho = sqrt(max(r * r - atmosphere.surface * atmosphere.surface, 0.0f));
    const float cosTheta = rho / r;
    const float theta = acos(cosTheta);
    const float muAngle = PI - theta;

    float cosZenith = 0.0f;
    float cosView = 0.0f;
    if (uv.y < 0.5f) {
        float coord = 1.0f - 2.0f * uv.y;
        coord = 1.0f - coord * coord;
        cosZenith = cos(muAngle * coord);
    } else {
        float coord = 1.0f - 2.0f * uv.y;
        coord = coord * coord;
        cosZenith = cos(muAngle + theta * coord);
    }
    cosView = -(uv.x * uv.x * 2.0f - 1.0f);

    return float2(cosZenith, cosView);
}

internal float2 skyViewUvFromZenCos(const AtmosphereSettings atmosphere, bool intersectsSurface, 
    float cosZenith, float cosView, float r) {

    const float rho = sqrt(max(r * r - atmosphere.surface * atmosphere.surface, 0.0f));
    const float cosTheta = rho / r;
    const float theta = acos(cosTheta);
    const float muAngle = PI - theta;

    float2 uv = float2(0.0f);

    if (!intersectsSurface) {
        float coord = acos(cosZenith) / muAngle;
        coord = 1.0f - coord;
        coord = 1.0f - sqrt(coord);
        uv.y = coord * 0.5f;
    } else {
        float coord = (acos(cosZenith) - muAngle) / theta;
        coord = sqrt(coord) + 1.0f;
        uv.y = coord * 0.5f;
    }
    float coord = -cosView * 0.5f + 0.5f;
    coord = sqrt(coord);
    uv.x = coord;

    uv = unitToSubUv(uv, float2(SKY_VIEW_LUT_WIDTH, SKY_VIEW_LUT_HEIGHT));

    return uv;
}

internal float getVisibility(const AtmosphereSettings atmosphere, float3 ro, float3 rd, float3 center) {
    const float surface = intersectSphere(ro, rd, center, atmosphere.surface).t;

    return surface == 0.0f ? 1.0f : 0.0f;
}

internal float2 multiscatteringUvFromRMu(const AtmosphereSettings atmosphere, float r, float mu) {
    float2 uv = float2(mu * 0.5f + 0.5f, (r - atmosphere.surface) / (atmosphere.atmosphere - atmosphere.surface));
    uv = unitToSubUv(saturate(uv), float2(MULTISCATTERING_LUT_RES));

    return uv;
}

internal float3 getSunLuminance(float3 ro, float3 rd, float3 sunDir, float surfaceRadius) {
    const float apertureDegrees = 0.545f;
    const float3 sunLuminance = float3(8e+6f);
    const float cosHalfApex = cos(0.5f * apertureDegrees * PI / 180.0f);
    const float cosViewSun = dot(rd, sunDir);

    if (cosViewSun > cosHalfApex) {
        const float depthSurface = intersectSphere(ro, rd, float3(0.0f), surfaceRadius).depth;
        if (depthSurface == 0.0f) {
            return sunLuminance * clamp(3 * (cosViewSun - cosHalfApex) / (cosHalfApex), 0.0, 1.0f);
        }
    }

    return float3(0.0f);
}

internal float aerialPerspectiveSliceToKm(float slice) {
    return slice * AERIAL_MM_PER_SLICE;
}

internal float aerialPerspectiveKmToSlice(float km) {
    return km / AERIAL_MM_PER_SLICE;
}

internal float3 expose(float3 color) {
    const float3 whitePoint = float3(1.08241f, 0.96756f, 0.95003f);
    const float exposure = 10.0f;
    return float3(1.0 - exp(-color.rgb / whitePoint * exposure));
}

