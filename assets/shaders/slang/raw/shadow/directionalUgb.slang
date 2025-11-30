module directionalShadow;

import "core/geometry";
import "core/viewInfo";
import "core/ugb";

struct Resources {
    ConstantBuffer<ViewInfo> view;
    UGB ugb;
    StructuredBuffer<RenderObject> renderObjects;
    StructuredBuffer<IndirectCommand> commands;
}

struct VSOutput
{
    float4 position : SV_POSITION;
};

ParameterBlock<Resources> resources;

[shader("vertex")]
VSOutput main(uint baseInstance : SV_StartInstanceLocation, uint vertexID: SV_VertexID) {
    const IndirectCommand command = resources.commands[baseInstance];
    const RenderObject renderObject = resources.renderObjects[command.renderObject];
    const float4x4 transform = renderObject.transform;
    const float3 position = resources.ugb.position(renderObject.positionIndex + command.vertexOffset + vertexID);

    VSOutput output;
    output.position = mul(mul(float4(position, 1.0f), transform), resources.view.camera.viewProjection);
    
    return output;
}