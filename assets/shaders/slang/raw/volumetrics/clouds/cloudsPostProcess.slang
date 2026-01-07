implementing volumetrics;

struct CloudsReprojectSamplers {
    [ImmutableSampler(SamplerFlags.ClampEdge | SamplerFlags.Linear)]
    SamplerState samplerEdge;

    [ImmutableSampler(SamplerFlags.Nearest)]
    SamplerState samplerNearest;
}

struct CloudsReprojectResources {
    Texture2D<float4> cloudColor;
    Texture2D<float2> cloudDepth;
    Texture2D sceneDepth;
    Texture2D<float4> cloudColorAccumulationIn;
    Texture2D<float2> cloudDepthAccumulationIn;
    Texture2D<float> cloudReprojectionFactorIn;
    RWTexture2D<float4> cloudColorAccumulationOut;
    RWTexture2D<float2> cloudDepthAccumulationOut;
    RWTexture2D<float>  cloudReprojectionFactorOut;

    ConstantBuffer<ViewInfo> view;
    ConstantBuffer<CloudsVPParameters> clouds;
}

[shader("compute")]
[numthreads(8, 8, 8)]
void cloudReprojection(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<CloudsReprojectSamplers> samplers,
    uniform ParameterBlock<CloudsReprojectResources> resources) {

    static const float2 REPROJECTION_FACTORS = float2(0.6f, 0.85f);
    
    const uint2 coord = int2(dispatchThreadID.xy);
    const uint2 renderCoord = coord / CLOUDS_REPROJECTION_RES;
    
    const uint2 drawnPixelCoord = 
        (coord / CLOUDS_REPROJECTION_RES) * CLOUDS_REPROJECTION_RES + 
        CLOUDS_REPROJECTION_OFFSETS[resources.view.frameNumberU32 % CLOUDS_REPROJECTION_RES_2D];
    const bool isDrawnPixel = all(coord == drawnPixelCoord);

    float2 imageSize;
    resources.cloudColorAccumulationOut.GetDimensions(imageSize.x, imageSize.y);
    const float2 uv = (float2(coord) + 0.5f) / imageSize;

    const float4 color = resources.cloudColor.SampleLevel(samplers.samplerNearest, uv, 0.0f);
    const float depth = resources.cloudDepth.SampleLevel(samplers.samplerNearest, uv, 0.0f).r;

    const float sceneDepth = resources.sceneDepth[coord].r;
    const float sceneDepthLinear = -Camera.linearizeReverseZInf(max(sceneDepth, 1e-7f), resources.view.camera.near);

    const float4 clip = float4(
        uv * 2.0f - 1.0f,
        -Camera.linearizeReverseZInf(max(depth, 1e-7f), resources.view.camera.near), 
        1
    );

    float4 unprojected = mul(clip, resources.view.camera.inverseProjection);
    unprojected /= unprojected.w;
    unprojected.xyz = mul(unprojected.xyz, float3x3(resources.view.camera.inverseView));

    float3 windDirection = float3(
        cos(resources.clouds.windAngle),
        -resources.clouds.windUprightAmount,
        sin(resources.clouds.windAngle)
    );

    //unprojected.xyz += layer.relativeHeight * windDirection * uWindHorizontalSkew;
    // todo: use delta time here and in main shader
    // todo: it does not seem to work for some reason...
    unprojected.xyz -= windDirection * resources.clouds.windSpeed;

    unprojected.xyz = mul(unprojected.xyz, float3x3(resources.view.previousCamera.view));
    float4 reprojected = mul(unprojected, resources.view.previousCamera.projection);
    reprojected /= reprojected.w;

    const float2 prevUv = reprojected.xy * 0.5f + 0.5f;

    const float4 prevColor = resources.cloudColorAccumulationIn.SampleLevel(samplers.samplerEdge, prevUv, 0.0f);
    const float2 prevDepth = resources.cloudDepthAccumulationIn.SampleLevel(samplers.samplerEdge, prevUv, 0.0f); 
    const float prevDepthLinearT = -Camera.linearizeReverseZInf(max(prevDepth.g, 1e-7f), resources.view.camera.near);

    const float prevReprojectionFactor = 
        resources.cloudReprojectionFactorIn.SampleLevel(samplers.samplerEdge, prevUv, 0.0f);

    if (coord.x >= imageSize.x || coord.y >= imageSize.y) {
        return;
    }

    if (resources.view.frameNumberU32 < 2 ||
        prevUv.x < 0.0f || prevUv.x > 1.0f || prevUv.y < 0.0f || prevUv.y > 1.0f) {

        const float4 color = resources.cloudColor.SampleLevel(samplers.samplerEdge, uv, 0.0f);
        const float depth = resources.cloudDepth.SampleLevel(samplers.samplerEdge, uv, 0.0f).r;

        resources.cloudColorAccumulationOut[coord] = color;
        resources.cloudDepthAccumulationOut[coord] = float2(depth, sceneDepth);
        resources.cloudReprojectionFactorOut[coord] = 0.05f;

        return;
    }

    float4 outColor = prevColor;
    float2 outDepth = prevDepth;
    float outReprojectionFactor = prevReprojectionFactor;

    if (isDrawnPixel) {
        if (abs(sceneDepthLinear - prevDepthLinearT) > sceneDepthLinear * 0.1f) {
            outColor = color;
            outDepth.x = depth;
            outReprojectionFactor = REPROJECTION_FACTORS.x;
        } else {
            outColor = varianceClampColor(renderCoord, outColor, (uint2)imageSize, resources.cloudColor);
            outColor = lerp(color, outColor, prevReprojectionFactor);
            outDepth.x = depth;
            outReprojectionFactor = getReprojectionFactor(prevReprojectionFactor, REPROJECTION_FACTORS.y); 
        }
    } else {
        if (abs(sceneDepthLinear - prevDepthLinearT) > sceneDepthLinear * 0.1f) {
            outColor = varianceClampColor(renderCoord, outColor, (uint2)imageSize, resources.cloudColor);
            outReprojectionFactor = 0;
        } else {
            outReprojectionFactor = getReprojectionFactor(prevReprojectionFactor, REPROJECTION_FACTORS.y); 
        }
    }

    outDepth.g = sceneDepth;
    resources.cloudColorAccumulationOut[coord] = outColor;
    resources.cloudDepthAccumulationOut[coord] = outDepth;
    resources.cloudReprojectionFactorOut[coord] = outReprojectionFactor;
}


struct CloudsComposeSamplers {
    [ImmutableSampler(SamplerFlags.ClampEdge | SamplerFlags.Linear)]
    SamplerState samplerEdge;
}

struct CloudsComposeResources {
    Texture2D<float4> cloudColor;
    Texture2D<float2> cloudDepth;
    Texture2D<float4> sceneColor;
    Texture2D<float> sceneDepth;
    RWTexture2D composed;
    ConstantBuffer<ViewInfo> view;
}

[shader("compute")]
[numthreads(8, 8, 8)]
void cloudComposition(
    uint2 dispatchThreadID: SV_DispatchThreadID,
    uniform ParameterBlock<CloudsComposeSamplers> samplers,
    uniform ParameterBlock<CloudsComposeResources> resources) {

    const uint2 coord = int2(dispatchThreadID.xy);

    float2 imageSize;
    resources.sceneColor.GetDimensions(imageSize.x, imageSize.y);
    const float2 uv = (float2(coord) + 0.5f) / imageSize;

    const float sceneDepth =
        -Camera.linearizeReverseZInf(
            resources.sceneDepth.SampleLevel(samplers.samplerEdge, uv, 0.0f), resources.view.camera.near);
    const float cloudsDepth = resources.cloudDepth.SampleLevel(samplers.samplerEdge, uv, 0.0f).r;
    
    const float3 sceneColor = resources.sceneColor.SampleLevel(samplers.samplerEdge, uv, 0.0f).rgb;

    if (cloudsDepth > sceneDepth) {
        resources.composed[coord] = float4(sceneColor, 1.0f);
    } else {
        const float4 cloudColor = resources.cloudColor.SampleLevel(samplers.samplerEdge, uv, 0.0f);
        resources.composed[coord] = float4(sceneColor * cloudColor.a + cloudColor.rgb, 1.0f);
    }
}




float getReprojectionFactor(float prev, float current) {
    return lerp(prev, current, 1.0f - exp(-0.07f));
}

float4 varianceClampColor(int2 renderCoord, float4 color, uint2 imageSize, Texture2D cloudColor) {
    float4 m1 = float4(0.0f);
    float4 m2 = float4(0.0f);
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            const int2 offset = int2(x, y);
            int2 neighbor = renderCoord + offset;
            neighbor = clamp(neighbor, int2(0), imageSize / CLOUDS_REPROJECTION_RES - 1);
            const float4 neightborColor = cloudColor[neighbor]; 
            m1 += neightborColor;
            m2 += neightborColor * neightborColor;
        }
    }

    const float4 mean = m1 / 9.0f;
    const float4 variance = (m2 / 9.0f) - (mean * mean);
    const float4 stddev = sqrt(max(variance, 0.0f));

    return clamp(color,  mean - stddev, mean + stddev);
}