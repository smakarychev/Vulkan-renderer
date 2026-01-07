implementing volumetrics;

import "utility/fullscreen";

struct AtmosphereRenderSamplers {
    [ImmutableSampler(SamplerFlags.ClampEdge | SamplerFlags.Linear)]
    SamplerState sampler;
}

struct AtmosphereRenderResources {
    Texture2D skyviewLut;
    ConstantBuffer<ViewInfo> view;
    
#if ATMOSPHERE_PRIMARY_VIEW
    Texture2D<float> depth;
    Texture3D aerialPerspectiveLut;
#endif // ATMOSPHERE_PRIMARY_VIEW
}

[shader("pixel")]
float4 atmosphereRender(FullScreenVSOutput input,
    uniform ParameterBlock<AtmosphereRenderSamplers> samplers,
    uniform ParameterBlock<AtmosphereRenderResources> resources) {

    const float3 clip = float3(input.uv * 2.0f - 1.0f, 1.0f);
    float4 unprojected = mul(float4(clip, 1.0f), resources.view.camera.inverseProjection);
    unprojected.xyz /= unprojected.w;
    const float3 rd = normalize(mul(float4(unprojected.xyz, 0.0f), resources.view.camera.inverseView).xyz);

    const float3 position = getViewPosition(resources.view.camera.position, resources.view.atmosphere.surface);
    const float3 sunDir = -resources.view.shading.primaryDirectionalLightDirection;
    
    float3 L = float3(0.0f);
    float depth = 0.0f;
#if ATMOSPHERE_PRIMARY_VIEW
    depth = resources.depth.SampleLevel(samplers.sampler, input.uv, 0);
#endif // ATMOSPHERE_PRIMARY_VIEW
    
    // draw the atmosphere behind the geometry
    if (depth == 0.0f) {
        const float r = length(position);
        if (r < resources.view.atmosphere.atmosphere) {
            const float3 up = position / r;
            const float mu = dot(rd, up);

            const float3 right = normalize(cross(up, rd));
            const float3 forward = normalize(cross(right, up));
            const float lightViewCos = normalize(float2(dot(sunDir, forward), dot(sunDir, right))).x;

            const bool intersectsSurface = 
                intersectSphere(position, rd, float3(0.0f), resources.view.atmosphere.surface).depth != 0.0f;

            const float2 skyViewUv = 
                skyViewUvFromZenCos(resources.view.atmosphere, intersectsSurface, mu, lightViewCos, r);
            
            L = resources.skyviewLut.SampleLevel(samplers.sampler, skyViewUv, 0).rgb;
#if ATMOSPHERE_PRIMARY_VIEW
            L += getSunLuminance(position, rd, sunDir, resources.view.atmosphere.surface) *
                 resources.view.shading.atmosphereTransmittanceAtView.rgb;
#endif // ATMOSPHERE_PRIMARY_VIEW

            return float4(L, 1.0f);
        }
    }

#if ATMOSPHERE_PRIMARY_VIEW
    const float linearDepth = -Camera.linearizeReverseZInf(depth, resources.view.camera.near);
    return getAerialPerspective(linearDepth, input.uv, resources.aerialPerspectiveLut, samplers.sampler);
#endif // ATMOSPHERE_PRIMARY_VIEW

    return float4(L, 1.0f);
}