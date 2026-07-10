#include "rendererpch.h"
#include "CopyToBufferPass.h"

#include "CopyBufferPass.h"
#include "FrameContext.h"
#include "UploadPass.h"
#include "RenderGraph/RGGraph.h"

Passes::CopyToBuffer::PassData& Passes::CopyToBuffer::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    
    const BufferResource source = Upload::addToGraph(name.Concatenate(".Upload"), renderGraph, info.Source);

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            passData.Destination = CopyBuffer::addToGraph(name.Concatenate(".Copy"), graph, {
                .Source = source,
                .Destination = info.Destination,
                .SizeBytes = info.Source.size(),
                .DestinationOffset = info.DestinationOffset
            }).Destination;
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}
