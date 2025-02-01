#include "BlitPass.h"

#include "Renderer.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Blit::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource textureIn,
    RG::Resource textureOut, const glm::vec3& offset, f32 relativeSize, ImageFilter filter)
{
    using namespace RG;
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            passData.TextureIn = graph.Read(textureIn,
                ResourceAccessFlags::Blit);

            passData.TextureOut = graph.Write(textureOut,
                ResourceAccessFlags::Blit);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Texture blit")
            
            auto&& [src, srcDescription] = resources.GetTextureWithDescription(passData.TextureIn);
            auto&& [dst, dstDescription] = resources.GetTextureWithDescription(passData.TextureOut);

            f32 srcAspect = (f32)srcDescription.Width / (f32)srcDescription.Height;
            f32 dstAspect = (f32)dstDescription.Width / (f32)dstDescription.Height;

            glm::uvec3 bottom = ImageUtils::getPixelCoordinates(dst, offset, ImageSizeType::Relative);
            f32 width = relativeSize;
            f32 height = relativeSize / srcAspect * dstAspect;
            glm::uvec3 top =
                ImageUtils::getPixelCoordinates(dst, offset + glm::vec3{width, height, 1.0f}, ImageSizeType::Relative);
            
            ImageCopyInfo srcBlit = {
                .Image = src,
                .Layers = 1,
                .Top = srcDescription.Dimensions()};
            ImageCopyInfo dstBlit = {
                .Image = dst,
                .Layers = 1,
                .Bottom = bottom,
                .Top = top};
            
            RenderCommand::BlitImage(frameContext.Cmd, srcBlit, dstBlit, filter);
        });

    return pass;
}
