module directionalShadow;

import "core/geometry";
import "core/viewInfo";
import "core/ugb";

import "scene/visibility/visibility";

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
    const float4x4 transform = renderObject.transform;
    const float3 position = resources.ugb.position(renderObject.positionIndex + meshlet.firstVertex + vertexID);

    VSOutput output;
    output.position = mul(mul(float4(position, 1.0f), transform), resources.view.camera.viewProjection);
    
    return output;
}