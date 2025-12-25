implementing shadow;

public struct CsmShadow : IShadowWorldSampler {
    CsmData csm;
    Texture2DArray shadowMap;
    SamplerComparisonState shadowSampler;
    float3 lightDirection;

    public __init(const CsmData csm, Texture2DArray shadowMap, SamplerComparisonState sampler, float3 lightDirection) {
        this.csm = csm;
        this.shadowMap = shadowMap;
        this.shadowSampler = sampler;
        this.lightDirection = lightDirection;
    }

    float sampleCascade(const IShadowSampler sampler, float3 ndc, float cascade) {
        const float constBias = 5e-3f;
        const float2 uv = ndc.xy * 0.5f + 0.5f;
        const float z = max(ndc.z + constBias, 1e-7f);
        ShadowArrayCtx ctx = ShadowArrayCtx(shadowMap, shadowSampler, cascade);
        
        return sampler.sample(ctx, uv, z);
    }

    public float sample(const IShadowSampler sampler, float3 positionWS, float3 normalWS, float viewZ) {
        const float SEEMS_THRESHOLD = 0.9f;

        viewZ = -viewZ;

        float3 shadowSize;
        shadowMap.GetDimensions(shadowSize.x, shadowSize.y, shadowSize.z);
        uint cascadeIndex = 0; 
        
        for (; cascadeIndex < csm.cascadeCount; cascadeIndex++) {
            if (viewZ < csm.cascades[cascadeIndex]) {
                break;
            }
        }
        if (cascadeIndex == csm.cascadeCount)
            return 0.0f;

        const float3 positionOffset = 
            shadowOffsetNormal(shadowSize.xy, normalWS, lightDirection) / csm.cascades[cascadeIndex];

        float4 positionLocal = mul(float4(positionWS + positionOffset, 1.0f), csm.viewProjections[cascadeIndex]);
        const float shadow = sampleCascade(sampler, positionLocal.xyz / positionLocal.w, cascadeIndex);

        // blend between cascades, if too close to the end of current cascade
        const uint nextCascade = cascadeIndex + 1;
        if (nextCascade >= csm.cascadeCount) {
            return shadow;
        }

        const float cascadeSplit = csm.cascades[cascadeIndex];
        const float cascadeSize = cascadeSplit - (cascadeIndex == 0 ? 0.0f : csm.cascades[cascadeIndex - 1]);
        const float cascadeRelativeDistance = 1.0 - (cascadeSplit - viewZ) / cascadeSize;

        if (cascadeRelativeDistance > SEEMS_THRESHOLD) {
            const float3 positionOffsetNext =
                shadowOffsetNormal(shadowSize.xy, normalWS, lightDirection) / csm.cascades[nextCascade];
            positionLocal = mul(float4(positionWS + positionOffsetNext, 1.0f), csm.viewProjections[nextCascade]);
            if (!all(bool3(
                abs(positionLocal.x) < 0.99f,
                abs(positionLocal.y) < 0.99f, 
                abs(positionLocal.z - 0.499f) < 0.499f)))
                return shadow;
            const float shadowNext = sampleCascade(sampler, positionLocal.xyz / positionLocal.w, nextCascade);
            
            return lerp(shadow, shadowNext, (cascadeRelativeDistance - SEEMS_THRESHOLD) / (1.0f - SEEMS_THRESHOLD));
        }

        return shadow;
    }
}