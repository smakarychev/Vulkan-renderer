implementing shadow;

import "core/viewInfo";

public struct CloudsShadow {
    ConstantBuffer<ViewInfo> view;
    Texture2D shadow;
    SamplerState shadowSampler;

    public __init(ConstantBuffer<ViewInfo> view, Texture2D shadow, SamplerState sampler) {
        this.view = view;
        this.shadow = shadow;
        this.shadowSampler = sampler;
    }

    public float sample(float3 positionWS) {
        float4 cloudShadowLocalPosition = mul(float4(positionWS, 1.0f), view.shading.volumetricCloudView);
        float4 cloudShadowProjected = mul(float4(positionWS, 1.0f), view.shading.volumetricCloudViewProjection);
        const float2 cloudShadowUv = (cloudShadowProjected.xy * 0.5f + 0.5f);
        const float3 cloudShadowSample = shadow.SampleLevel(shadowSampler, cloudShadowUv, 0).rgb;
        float cloudShadow = min(cloudShadowSample.b,
            cloudShadowSample.g * max(0, cloudShadowSample.r - cloudShadowLocalPosition.z));
        
        return (1.0f - exp(-cloudShadow)) * view.shading.volumetricCloudShadowStrength;
    }
}