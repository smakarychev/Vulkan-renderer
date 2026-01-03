module viewInfo;

import "attributes";
import "light/light";

public static const uint VIEW_IS_ORTHOGRAPHIC_BIT = 0;
public static const uint VIEW_CLAMP_DEPTH_BIT = 1;

[StandaloneType("camera")]
public struct Camera {
    public bool isOrthographic() { return ((viewFlags >> VIEW_IS_ORTHOGRAPHIC_BIT) & 1u) == 1; }
    public bool isDepthClamped() { return ((viewFlags >> VIEW_CLAMP_DEPTH_BIT) & 1u) == 1; }

    public float3 screenToView(float3 screen) {
        const float2 ndc = screen.xy / resolution;
        float4 clip = float4(ndc * 2.0f - 1.0f, screen.z, 1.0f);
        const float4 view = mul(clip, inverseProjection);

        return view.xyz / view.w;
    }

    public static float linearizeReverseZInf(float z, float n) { return -n / z; }
    public static float linearizeReverseZ(float z, float n, float f) { return f * n / ((n - f) * z - n); }
    public static float linearizeReverseZOrtho(float z, float n, float f) { return (f - n) * z - f; }

    public float4x4 viewProjection;
    public float4x4 projection;
    public float4x4 view;

    public float3 position;
    public float near;
    public float3 forward;
    public float far;
    public float3 right;
    public float fov;
    public float3 up;
    public float aspectRatio;

    public float4x4 inverseViewProjection;
    public float4x4 inverseProjection;
    public float4x4 inverseView;

    public float frustumTopY;
    public float frustumTopZ;
    public float frustumRightX;
    public float frustumRightZ;
    public float frustumNear;
    public float frustumFar;
    public float projectionWidth;
    public float projectionHeight;
    public float projectionBiasX;
    public float projectionBiasY;
    
    public float2 resolution;
    public float2 hizResolution;
    public uint viewFlags;
    public uint visibilityFlags;
}

[StandaloneType("atmosphereSettings")]
public struct AtmosphereSettings {
    public static const float EARTH_RADIUS_DEFAULT_KM = 6360.0f;

    public float4 rayleighScattering;
    public float4 rayleighAbsorption;
    public float4 mieScattering;
    public float4 mieAbsorption;
    public float4 ozoneAbsorption;
    public float4 surfaceAlbedo;

    public float surface;
    public float atmosphere;
    public float rayleighDensity;
    public float mieDensity;
    public float ozoneDensity;
}

[StandaloneType("shadingSettings")]
public struct ShadingSettings {
    public float environmentPower;
    public bool softShadows;
    public float maxLightCullDistance;
    public float volumetricCloudShadowStrength; 
    public uint transmittanceLut;
    public uint skyViewLut;
    public uint volumetricCloudShadow;

    public uint directionalLightCount;
    public uint pointLightCount;
    public bool lightCullingUseZBins;
    public uint2 lightCullTileCount;

    public float4 atmosphereTransmittanceAtView;
    public float3 primaryDirectionalLightDirection;
    public float primaryDirectionalLightIntensity;
    public float4 primaryDirectionalLightColor;

    public float4x4 volumetricCloudViewProjection;
    public float4x4 volumetricCloudView;
}

[StandaloneType("viewInfo")]
public struct ViewInfo {
    public Camera camera;
    public Camera previousCamera;
    public AtmosphereSettings atmosphere;
    uint unused0[3];

    public ShadingSettings shading;

    public float frameNumber;
    public uint frameNumberU32;
    uint unused1[2];
};