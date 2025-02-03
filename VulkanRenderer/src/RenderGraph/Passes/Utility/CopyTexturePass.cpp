#include "CopyTexturePass.h"

#include "FrameContext.h"

RG::Pass& Passes::CopyTexture::addToGraph(std::string_view name, RG::Graph& renderGraph,
    RG::Resource textureIn, RG::Resource textureOut,
    const glm::vec3& offset, const glm::vec3& size, ImageSizeType sizeType)
{
    using namespace RG;
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            passData.TextureIn = graph.Read(textureIn,
                ResourceAccessFlags::Copy);

            passData.TextureOut = graph.Write(textureOut,
                ResourceAccessFlags::Copy);

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
            
            switch (sizeType)
            {
            case ImageSizeType::Absolute:
                dstSubregion = {
                    .Layers = 1,
                    .Bottom = offset,
                    .Top = offset + size};
                break;
            case ImageSizeType::Relative:
                dstSubregion = {
                    .Layers = 1,
                    .Bottom = ImageUtils::getPixelCoordinates(dst, offset, ImageSizeType::Relative),
                    .Top = ImageUtils::getPixelCoordinates(dst, offset + size, ImageSizeType::Relative)};
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
