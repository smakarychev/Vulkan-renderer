module vbufferDraw;

import "core/attributes";
import "core/geometry";
import "core/materials/material";
import "core/texturesHeap";
import "core/viewInfo";
import "core/ugb";

import "scene/vbuffer/vbuffer";
import "scene/visibility/visibility";

struct Samplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct Resources {
    ConstantBuffer<ViewInfo> view;
    UGB ugb;
    UGBMeshlets meshlets;
    StructuredBuffer<RenderObject> renderObjects;
    StructuredBuffer<SceneVisibilityElement> visibleMeshlets;
    StructuredBuffer<StandardPbrMaterial> materials;
}

struct VSOutput
{
    float4 position : SV_POSITION;
    nointerpolation uint instanceId;
    nointerpolation uint materialId;
    float2 uv;
};

ParameterBlock<Samplers> samplers;
ParameterBlock<Resources> resources;

[shader("vertex")]
VSOutput vertexMain(uint baseInstance: SV_StartInstanceLocation, uint vertexID: SV_VertexID) {
    const SceneVisibilityElement element = resources.visibleMeshlets[baseInstance];
    const RenderObject renderObject = resources.renderObjects[element.renderObjectId];
    const Meshlet meshlet = resources.meshlets.meshlet(renderObject.meshletIndex + element.meshletId);
    const float4x4 transform = renderObject.transform;
    const float3 position = resources.ugb.position(renderObject.positionIndex + meshlet.firstVertex + vertexID);

    VSOutput output;
    output.instanceId = baseInstance;
    output.materialId = renderObject.materialId;
    output.uv = resources.ugb.uv(renderObject.uvIndex + meshlet.firstVertex + vertexID);
    output.position = mul(mul(float4(position, 1.0f), transform), resources.view.camera.viewProjection);
    
    return output;
}

[shader("pixel")]
uint pixelMain(VSOutput input, uint primitiveID: SV_PrimitiveID) {
    const StandardPbrMaterial material = resources.materials[input.materialId];

    float alpha = material.albedo.a;
    alpha *= nonuniform(material.albedoTexture.desc).Sample(samplers.sampler, input.uv).a;

    if (alpha < 0.5f)
        discard;

    const VBufferVisibility visibility = VBufferVisibility(input.instanceId, primitiveID);
    
    return visibility.pack();
}