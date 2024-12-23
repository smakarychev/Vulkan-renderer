#include "MipMapPass.h"

#include "FrameContext.h"
#include "RenderGraph/RenderGraph.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Mipmap::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource texture)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
    [&](Graph& graph, PassData& passData)
    {
        CPU_PROFILE_FRAME("MipMap.Setup")

        passData.Texture = graph.Write(texture, Blit);

        graph.UpdateBlackboard(passData);
    },
    [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
    {
        CPU_PROFILE_FRAME("MipMap")
        GPU_PROFILE_FRAME("MipMap")

        // todo: nvpro mipmap software generation?
        const Texture& sourceTexture = resources.GetTexture(passData.Texture);
        sourceTexture.CreateMipmaps(frameContext.Cmd, ImageLayout::Destination);
        DependencyInfo layoutTransition = DependencyInfo::Builder()
            .LayoutTransition({
                .ImageSubresource = sourceTexture.Subresource(),
                .SourceStage = PipelineStage::Blit,
                .DestinationStage = PipelineStage::Blit,
                .SourceAccess = PipelineAccess::ReadTransfer,
                .DestinationAccess = PipelineAccess::WriteTransfer,
                .OldLayout = ImageLayout::Source,
                .NewLayout = ImageLayout::Destination})
            .Build(frameContext.DeletionQueue);
        RenderCommand::WaitOnBarrier(frameContext.Cmd, layoutTransition);
    });
}
