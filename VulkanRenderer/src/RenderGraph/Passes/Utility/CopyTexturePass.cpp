#include "rendererpch.h"

#include "CopyTexturePass.h"

#include "FrameContext.h"
#include "Rendering/Image/ImageUtility.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGUtils.h"

Passes::CopyTexture::PassData& Passes::CopyTexture::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            const ImageResource copy = RgUtils::ensureResource(info.TextureOut, graph, name.Concatenate("Copy"),
                RGImageDescription{
                    .Inference = RGImageInference::Full,
                    .Reference = info.TextureIn,
                });
            
            passData.TextureIn = graph.ReadImage(info.TextureIn, ResourceAccessFlags::Copy);
            passData.TextureOut = graph.WriteImage(copy, ResourceAccessFlags::Copy);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            GPU_PROFILE_FRAME("Texture copy")

            auto&& [src, srcDescription] = graph.GetImageWithDescription(passData.TextureIn);
            auto&& [dst, dstDescription] = graph.GetImageWithDescription(passData.TextureOut);

            ImageSubregion srcSubregion = {
                .Layers = 1,
                .Top = srcDescription.Dimensions()
            };
            ImageSubregion dstSubregion = {};

            switch (info.SizeType)
            {
            case ImageSizeType::Absolute:
                dstSubregion = {
                    .Layers = 1,
                    .Bottom = info.Offset,
                    .Top = info.Offset + info.Size
                };
                break;
            case ImageSizeType::Relative:
                dstSubregion = {
                    .Layers = 1,
                    .Bottom = images::getPixelCoordinates(dst, info.Offset, ImageSizeType::Relative),
                    .Top = images::getPixelCoordinates(dst, info.Offset + info.Size, ImageSizeType::Relative)
                };
                break;
            }

            frameContext.CommandList.CopyImage({
                .Source = src,
                .Destination = dst,
                .SourceSubregion = srcSubregion,
                .DestinationSubregion = dstSubregion
            });
        });
}
