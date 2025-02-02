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
        Texture sourceTexture = resources.GetTexture(passData.Texture);
        Device::CreateMipmaps(sourceTexture, frameContext.Cmd, ImageLayout::Destination);
        RenderCommand::WaitOnBarrier(frameContext.Cmd, Device::CreateDependencyInfo({
            .LayoutTransitionInfo = LayoutTransitionInfo{
                .ImageSubresource = ImageSubresource{.Image = sourceTexture},
                .SourceStage = PipelineStage::Blit,
                .DestinationStage = PipelineStage::Blit,
                .SourceAccess = PipelineAccess::ReadTransfer,
                .DestinationAccess = PipelineAccess::WriteTransfer,
                .OldLayout = ImageLayout::Source,
                .NewLayout = ImageLayout::Destination}},
            frameContext.DeletionQueue));
    });
}
