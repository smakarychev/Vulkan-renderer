#include "BlitPass.h"

#include "Renderer.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Blit::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource textureIn,
    RG::Resource textureOut, const glm::vec3& offset, f32 relativeSize)
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
            
            const Texture& src = resources.GetTexture(passData.TextureIn);
            const Texture& dst = resources.GetTexture(passData.TextureOut);

            f32 srcAspect = (f32)src.Description().Width / (f32)src.Description().Height;
            f32 dstAspect = (f32)dst.Description().Width / (f32)dst.Description().Height;

            glm::uvec3 bottom = dst.GetPixelCoordinate(offset, ImageSizeType::Relative);
            f32 width = relativeSize;
            f32 height = relativeSize / srcAspect * dstAspect;
            glm::uvec3 top = dst.GetPixelCoordinate(offset + glm::vec3{width, height, 1.0f}, ImageSizeType::Relative);
            
            ImageCopyInfo srcBlit = src.CopyInfo();
            ImageCopyInfo dstBlit = dst.BlitInfo(bottom, top, 0, 0, 1);
            
            RenderCommand::BlitImage(frameContext.Cmd, srcBlit, dstBlit, ImageFilter::Linear);
        });

    return pass;
}
