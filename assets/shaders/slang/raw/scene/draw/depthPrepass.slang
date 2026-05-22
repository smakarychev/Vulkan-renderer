module depthPrepass;

import "core/attributes";
import "core/geometry";
import "core/materials/material";
import "core/texturesHeap";
import "core/viewInfo";
import "core/ugb";

import "scene/visibility/visibility";
import "skinning/skinning";

struct Samplers {
    [ImmutableSampler(SamplerFlags.Linear)]
    SamplerState sampler;
}

struct Resources {
    ConstantBuffer<ViewInfo> view;
    UGB ugb;
    UGBMeshlets meshlets;
    StructuredBuffer<RenderObject> renderObjects;
    StructuredBuffer<SceneDrawElement> visibleMeshlets;
    StructuredBuffer<StandardPbrMaterial> materials;
}

struct VSOutput
{
    float4 position : SV_POSITION;
    nointerpolation uint materialId;
    float2 uv;
};

ParameterBlock<Samplers> samplers;
ParameterBlock<Resources> resources;

[shader("vertex")]
VSOutput vertexMain(uint baseInstance : SV_StartInstanceLocation, uint vertexID: SV_VertexID) {
    const SceneDrawElement element = resources.visibleMeshlets[baseInstance];
    const RenderObject renderObject = resources.renderObjects[element.renderObjectId];
    const Meshlet meshlet = resources.meshlets.meshlet(renderObject.meshletIndex + element.meshletId);
    const uint vertexOffset = meshlet.firstVertex + vertexID;
    float4x4 transform = renderObject.transform;
    
    float4 position;
    [branch]
    if (WaveActiveBitOr(renderObject.skinIndex != ~0u)) {
        SkinnedVertex vertex = resources.ugb.skinnedVertex(renderObject.positionIndex + vertexOffset);
        position = float4(vertex.position, 1.0f);
    }
    else {
        position = mul(float4(resources.ugb.position(renderObject.positionIndex + vertexOffset), 1.0f), transform);
    }

    VSOutput output;
    output.materialId = renderObject.materialId;
    output.position = mul(position, resources.view.camera.viewProjection);
#if ALPHA_TEST    
    output.uv = resources.ugb.uv(renderObject.uvIndex + command.vertexOffset + vertexID);
#endif // ALPHA_TEST
    
    return output;
}

[shader("pixel")]
void pixelMain(VSOutput input) {
#if ALPHA_TEST
    const StandardPbrMaterial material = resources.materials[input.materialId];
    if (textures.textures[NonUniformResourceIndex(material.albedoTexture)].Sample(samplers.sampler, input.uv).a < 0.5f)
        discard;
#endif // ALPHA_TEST
}