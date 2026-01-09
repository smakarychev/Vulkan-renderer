module depthPrepass;

import "core/attributes";
import "core/geometry";
import "core/materials/material";
import "core/viewInfo";
import "core/ugb";

struct Samplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct Resources {
    ConstantBuffer<ViewInfo> view;
    UGB ugb;
    StructuredBuffer<RenderObject> renderObjects;
    StructuredBuffer<IndirectCommand> commands;
}

struct VSOutput
{
    float4 position : SV_POSITION;
    nointerpolation uint materialId;
    float2 uv;
};

ParameterBlock<Samplers> samplers;
ParameterBlock<Resources> resources;
ParameterBlock<StandardPbrMaterials> materials;

[shader("vertex")]
VSOutput vertexMain(uint baseInstance : SV_StartInstanceLocation, uint vertexID: SV_VertexID) {
    const IndirectCommand command = resources.commands[baseInstance];
    const RenderObject renderObject = resources.renderObjects[command.renderObject];
    const float4x4 transform = renderObject.transform;
    const float3 position = resources.ugb.position(renderObject.positionIndex + command.vertexOffset + vertexID);

    VSOutput output;
    output.materialId = renderObject.materialId;
    output.position = mul(mul(float4(position, 1.0f), transform), resources.view.camera.viewProjection);
#if DEPTH_PREPASS_ALPHA_TEST    
    output.uv = resources.ugb.uv(renderObject.uvIndex + command.vertexOffset + vertexID);
#endif // DEPTH_PREPASS_ALPHA_TEST
    
    return output;
}

[shader("pixel")]
void pixelMain(VSOutput input) {
#if DEPTH_PREPASS_ALPHA_TEST
    const StandardPbrMaterial material = materials.materials[input.materialId];
    if (materials.textures[NonUniformResourceIndex(material.albedoTexture)].Sample(samplers.sampler, input.uv).a < 0.5f)
        discard;
#endif // DEPTH_PREPASS_ALPHA_TEST
}