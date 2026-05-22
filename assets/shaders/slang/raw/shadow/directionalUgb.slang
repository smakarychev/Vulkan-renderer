module directionalShadow;

import "core/geometry";
import "core/viewInfo";
import "core/ugb";

import "scene/visibility/visibility";
import "skinning/skinning";

struct Resources {
    ConstantBuffer<ViewInfo> view;
    UGB ugb;
    UGBMeshlets meshlets;
    StructuredBuffer<RenderObject> renderObjects;
    StructuredBuffer<SceneDrawElement> visibleMeshlets;
}

struct VSOutput
{
    float4 position : SV_POSITION;
};

ParameterBlock<Resources> resources;

[shader("vertex")]
VSOutput main(uint baseInstance : SV_StartInstanceLocation, uint vertexID: SV_VertexID) {
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
    output.position = mul(position, resources.view.camera.viewProjection);
    
    return output;
}