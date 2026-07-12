#include "rendererpch.h"
#include "CopyBufferPass.h"

#include "FrameContext.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGUtils.h"

Passes::CopyBuffer::PassData& Passes::CopyBuffer::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            const BufferResource copy = RgUtils::ensureResource(info.Destination, graph, name.Concatenate(".Copy"),
                RGBufferDescription{
                    .SizeBytes = graph.GetBufferDescription(info.Source).SizeBytes
                });
            
            passData.Source = graph.ReadBuffer(info.Source, ResourceAccessFlags::Copy);
            passData.Destination = graph.WriteBuffer(copy, ResourceAccessFlags::Copy);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            GPU_PROFILE_FRAME("Buffer copy")
            
            const Buffer source = graph.GetBuffer(passData.Source);
            const Buffer destination = graph.GetBuffer(passData.Destination);
            auto& cmd = frameContext.CommandList;
            cmd.CopyBuffer(CopyBufferCommand{
                .Source = source, 
                .Destination = destination, 
                .SizeBytes = info.SizeBytes,
                .SourceOffset = info.SourceOffset,
                .DestinationOffset = info.DestinationOffset
            });
        });
}
