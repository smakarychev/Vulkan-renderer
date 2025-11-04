module viewInfo;

import "attributes";

static const uint VIEW_IS_ORTHOGRAPHIC_BIT = 0;
static const uint VIEW_CLAMP_DEPTH_BIT = 1;

[StandaloneType("camera")]
public struct Camera {
    public bool isOrthographic() { return ((viewFlags >> VIEW_IS_ORTHOGRAPHIC_BIT) & 1u) == 1; }
    public bool isDepthClamped() { return ((viewFlags >> VIEW_CLAMP_DEPTH_BIT) & 1u) == 1; }
    public float linearizeReverseZ(float z) { return -near / z;}

    public float4x4 viewProjection;
    public float4x4 projection;
    public float4x4 view;

    public float3 position;
    public float near;
    public float3 forward;
    public float far;

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
    uint unused4;

    public float4x4 volumetricCloudViewProjection;
    public float4x4 volumetricCloudView;
}

[StandaloneType("viewInfo")]
public struct ViewInfo {
    public Camera camera;
    public Camera previousCamera;
    public AtmosphereSettings atmosphere;
    uint unused0[3];

    ShadingSettings shading;

    public float frameNumber;
    public uint frameNumberU32;
    uint unused1[2];
};