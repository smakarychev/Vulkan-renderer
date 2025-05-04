#include "BlitPass.h"

#include "Renderer.h"

Passes::Blit::PassData& Passes::Blit::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn,
    RG::Resource textureOut, const glm::vec3& offset, f32 relativeSize, ImageFilter filter)
{
    using namespace RG;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            passData.TextureIn = graph.Read(textureIn,
                ResourceAccessFlags::Blit);

            passData.TextureOut = graph.Write(textureOut,
                ResourceAccessFlags::Blit);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Texture blit")
            
            auto&& [src, srcDescription] = resources.GetTextureWithDescription(passData.TextureIn);
            auto&& [dst, dstDescription] = resources.GetTextureWithDescription(passData.TextureOut);

            f32 srcAspect = (f32)srcDescription.Width / (f32)srcDescription.Height;
            f32 dstAspect = (f32)dstDescription.Width / (f32)dstDescription.Height;

            glm::uvec3 bottom = Images::getPixelCoordinates(dst, offset, ImageSizeType::Relative);
            f32 width = relativeSize;
            f32 height = relativeSize / srcAspect * dstAspect;
            glm::uvec3 top =
                Images::getPixelCoordinates(dst, offset + glm::vec3{width, height, 1.0f}, ImageSizeType::Relative);
            
            frameContext.CommandList.BlitImage({
                .Source = src,
                .Destination = dst,
                .Filter = filter,
                .SourceSubregion = {
                    .Layers = 1,
                    .Top = srcDescription.Dimensions()},
                .DestinationSubregion = {
                    .Layers = 1,
                    .Bottom = bottom,
                    .Top = top}});
        }).Data;
}
