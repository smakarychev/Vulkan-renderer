module shadow;

__include "pcfShadowSampler";
__include "csmShadow";
__include "shadowUtility";

import "core/attributes";

[StandaloneType("csmData")]
public struct CsmData {
    public static const uint MAX_CASCADES = 5;

    public uint cascadeCount;
    public float cascades[MAX_CASCADES];
    public float4x4 viewProjections[MAX_CASCADES];
    public float4x4 views[MAX_CASCADES];
    public float near[MAX_CASCADES];
    public float far[MAX_CASCADES];
}

public struct CsmResources {
    public ConstantBuffer<CsmData> csmData;
    public Texture2DArray csmTexture;
}

public interface IShadowCtx {
    public float sample(float2 uv, float z);
    public float2 shadowMapSize();
}

public struct ShadowCtx : IShadowCtx {
    Texture2D shadowMap;
    SamplerComparisonState shadowSampler;

    public __init(Texture2D map, SamplerComparisonState sampler) {
        shadowMap = map;
        shadowSampler = sampler;
    }

    public float sample(float2 uv, float z) {
        return shadowMap.SampleCmpLevelZero(shadowSampler, uv, z);
    }

    public float2 shadowMapSize() {
        float2 size;
        shadowMap.GetDimensions(size.x, size.y);

        return size;
    }
}

public struct ShadowArrayCtx : IShadowCtx {
    Texture2DArray shadowMap;
    SamplerComparisonState shadowSampler;
    float layer;

    public __init(Texture2DArray map, SamplerComparisonState sampler, float layer) {
        shadowMap = map;
        shadowSampler = sampler;
        this.layer = layer;
    }

    public float sample(float2 uv, float z) {
        return shadowMap.SampleCmpLevelZero(shadowSampler, float3(uv, layer), z);
    }

    public float2 shadowMapSize() {
        float3 size;
        shadowMap.GetDimensions(size.x, size.y, size.z);

        return size.xy;
    }
}

public interface IShadowSampler {
    public float sample(const IShadowCtx ctx, float2 uv, float z);
}

public interface IShadowWorldSampler {
    public float sample(const IShadowSampler sampler, float3 positionWS, float3 normalWS, float viewZ);
}