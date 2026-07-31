module momentBasedOITComposite;

// https://momentsingraphics.de/I3D2018.html

struct CompositePassResources {
    Texture2D<float16_t> absorbanceTotal;
    Texture2D<float16_t4> translucent;
    RWTexture2D<float16_t4> sceneColor;
}

ParameterBlock<CompositePassResources> resources;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint2 dispatchThreadID: SV_DispatchThreadID) {
    const uint2 coord = int2(dispatchThreadID.xy);

    const float b0 = resources.absorbanceTotal[coord];
    const float totalTransmittance = exp(-b0);
    const float3 sceneColor = resources.sceneColor[coord].rgb;
    const float4 translucent = resources.translucent[coord];
    const float renormalization = 1.0f / max(translucent.a, 1e-8f);

    const float3 composited = lerp(translucent.rgb * renormalization, sceneColor, totalTransmittance);

    resources.sceneColor[coord] = float16_t4(composited, 1.0f);
}
