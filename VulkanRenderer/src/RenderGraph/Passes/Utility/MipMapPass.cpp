#include "rendererpch.h"

#include "MipMapPass.h"

#include "FrameContext.h"
#include "RenderGraph/RGGraph.h"

Passes::Mipmap::PassData& Passes::Mipmap::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource texture)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("MipMap.Setup")

            passData.Texture = graph.WriteImage(texture, Blit);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("MipMap")
            GPU_PROFILE_FRAME("MipMap")

            // todo: nvpro mipmap software generation?
            Texture sourceTexture = graph.GetImage(passData.Texture);
            Device::CreateMipmaps(sourceTexture, frameContext.CommandList, ImageLayout::Destination);
            frameContext.CommandList.WaitOnBarrier({
                .DependencyInfo = Device::CreateDependencyInfo({
                    .LayoutTransitionInfo = LayoutTransitionInfo{
                        .ImageSubresource = ImageSubresource{.Image = sourceTexture},
                        .SourceStage = PipelineStage::Blit,
                        .DestinationStage = PipelineStage::Blit,
                        .SourceAccess = PipelineAccess::ReadTransfer,
                        .DestinationAccess = PipelineAccess::WriteTransfer,
                        .OldLayout = ImageLayout::Source,
                        .NewLayout = ImageLayout::Destination}},
                    frameContext.DeletionQueue)});
        });
}
