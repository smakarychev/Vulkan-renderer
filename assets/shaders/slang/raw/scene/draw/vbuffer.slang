module vbufferDraw;

import "scene/vbuffer/vbuffer";
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
    nointerpolation uint commandId;
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
    output.commandId = baseInstance;
    output.materialId = renderObject.materialId;
    output.uv = resources.ugb.uv(renderObject.uvIndex + command.vertexOffset + vertexID);
    output.position = mul(mul(float4(position, 1.0f), transform), resources.view.camera.viewProjection);
    
    return output;
}

[shader("pixel")]
uint pixelMain(VSOutput input, uint primitiveID: SV_PrimitiveID) {
    const IndirectCommand command = resources.commands[input.commandId];
    const StandardPbrMaterial material = materials.materials[input.materialId];

    float alpha = material.albedo.a;
    alpha *= materials.textures[NonUniformResourceIndex(material.albedoTexture)].Sample(samplers.sampler, input.uv).a;

    if (alpha < 0.5f)
        discard;

    const VBufferVisibility visibility = VBufferVisibility(command.firstInstance, primitiveID);
    
    return visibility.pack();
}