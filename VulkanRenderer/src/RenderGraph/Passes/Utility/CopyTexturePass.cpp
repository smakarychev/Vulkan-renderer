#include "CopyTexturePass.h"

#include "FrameContext.h"

RG::Pass& Passes::CopyTexture::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            passData.TextureIn = graph.Read(info.TextureIn, ResourceAccessFlags::Copy);
            passData.TextureOut = graph.Write(info.TextureOut, ResourceAccessFlags::Copy);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Texture copy")

            auto&& [src, srcDescription] = resources.GetTextureWithDescription(passData.TextureIn);
            auto&& [dst, dstDescription] = resources.GetTextureWithDescription(passData.TextureOut);

            ImageSubregion srcSubregion = {
                .Layers = 1,
                .Top = srcDescription.Dimensions()};
            ImageSubregion dstSubregion = {};
            
            switch (info.SizeType)
            {
            case ImageSizeType::Absolute:
                dstSubregion = {
                    .Layers = 1,
                    .Bottom = info.Offset,
                    .Top = info.Offset + info.Size};
                break;
            case ImageSizeType::Relative:
                dstSubregion = {
                    .Layers = 1,
                    .Bottom = Images::getPixelCoordinates(dst, info.Offset, ImageSizeType::Relative),
                    .Top = Images::getPixelCoordinates(dst, info.Offset + info.Size, ImageSizeType::Relative)};
                break;
            }

            frameContext.CommandList.CopyImage({
                .Source = src,
                .Destination = dst,
                .SourceSubregion = srcSubregion,
                .DestinationSubregion = dstSubregion});
        });

    return pass;
}
