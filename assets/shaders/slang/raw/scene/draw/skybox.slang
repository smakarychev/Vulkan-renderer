module skybox;

import "core/attributes";
import "core/viewInfo";

struct Samplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct Resources {
    ConstantBuffer<ViewInfo> view;
    TextureCube<float4> skybox;
}

struct VSOutput {
    float4 position : SV_Position;
    float3 cubeUv;
}

ParameterBlock<Samplers> samplers;
ParameterBlock<Resources> resources;

[shader("vertex")]
VSOutput vertexMain(uint vertexID: SV_VertexID) {
    static const float2 vertices[6] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  1.0f),
        float2( 1.0f,  1.0f),

        float2(-1.0f, -1.0f),
        float2( 1.0f,  1.0f),
        float2( 1.0f, -1.0f),
    };

    VSOutput output;
    output.position = float4(vertices[vertexID], 0.0f, 1.0f);
    const float3 unprojected = mul(output.position, resources.view.camera.inverseProjection).xyz;
    output.cubeUv = mul(unprojected, float3x3(resources.view.camera.inverseView));

    return output;
}

[shader("pixel")]
float4 pixelMain(VSOutput input, uniform float lodBias) {
    const float3 color = resources.skybox.Sample(samplers.sampler, input.cubeUv, 0).rgb * 
        resources.view.shading.fixedExposureToExposureMultiplier;
    
    return float4(color, 1.0f);
}
