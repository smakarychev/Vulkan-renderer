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

    public float sample(const IShadowSampler sampler, float3 positionWS, float3 normalWS) {
        const float SEEMS_THRESHOLD = 0.05f;
        const float EPS = 1e-2f;

        float3 shadowSize;
        shadowMap.GetDimensions(shadowSize.x, shadowSize.y, shadowSize.z);
        uint cascadeIndex = 0;

        float4 shadowNdc;
        [unroll]
        for (; cascadeIndex < csm.cascadeCount; cascadeIndex++) {
            shadowNdc = mul(float4(positionWS, 1.0f), csm.viewProjections[cascadeIndex]);

            if (abs(shadowNdc.x) + EPS <= 1.0f &&
                abs(shadowNdc.y) + EPS <= 1.0f &&
                abs(shadowNdc.z - 0.5f) + EPS < 0.5f) 
                break; 
        }

        if (cascadeIndex == csm.cascadeCount)
            return 0.0f;

        const float distanceX = 1.0f - abs(shadowNdc.x);
        const float distanceY = 1.0f - abs(shadowNdc.y);
        const float distanceZ = 1.0f - abs(shadowNdc.z * 2.0f - 1.0f);
        const float distToCascadeEdge = min(min(distanceX, distanceY), distanceZ);

        const float3 positionOffset = 
            shadowOffsetNormal(shadowSize.xy, normalWS, lightDirection) / csm.cascades[cascadeIndex];

        float4 positionLocal = mul(float4(positionWS + positionOffset, 1.0f), csm.viewProjections[cascadeIndex]);
        const float shadow = sampleCascade(sampler, positionLocal.xyz / positionLocal.w, cascadeIndex);

        // blend between cascades, if too close to the end of current cascade
        const uint nextCascade = cascadeIndex + 1;
        if (nextCascade >= csm.cascadeCount) {
            return shadow;
        }

        [branch]
        if (distToCascadeEdge <= SEEMS_THRESHOLD) {
            const float4 nextShadowNdc = mul(float4(positionWS, 1.0f), csm.viewProjections[nextCascade]);

            if (abs(nextShadowNdc.x) + EPS < 1.0f &&
                abs(nextShadowNdc.y) + EPS < 1.0f &&
                abs(nextShadowNdc.z - 0.5f) + EPS < 0.5f) {
                const float3 positionOffsetNext = 
                    shadowOffsetNormal(shadowSize.xy, normalWS, lightDirection) / csm.cascades[nextCascade];

                positionLocal = mul(float4(positionWS + positionOffsetNext, 1.0f), csm.viewProjections[nextCascade]);
                const float shadowNext = sampleCascade(sampler, positionLocal.xyz / positionLocal.w, nextCascade);

                return lerp(shadowNext, shadow, smoothstep(0.0f, SEEMS_THRESHOLD, distToCascadeEdge));
            }
        }

        return shadow;
    }
}