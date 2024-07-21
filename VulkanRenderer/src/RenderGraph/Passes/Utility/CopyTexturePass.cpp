#include "CopyTexturePass.h"

#include "FrameContext.h"
#include "Vulkan/RenderCommand.h"

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

            graph.GetBlackboard().Update(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Texture copy")
            
            const Texture& src = resources.GetTexture(passData.TextureIn);
            const Texture& dst = resources.GetTexture(passData.TextureOut);

            ImageCopyInfo srcCopy = src.CopyInfo();
            ImageCopyInfo dstCopy = {};
            
            switch (sizeType)
            {
            case ImageSizeType::Absolute:
                dstCopy = dst.CopyInfo(offset, offset + size, 0, 0, 1);
                break;
            case ImageSizeType::Relative:
                dstCopy = dst.CopyInfo(
                    dst.GetPixelCoordinate(offset, ImageSizeType::Relative),
                    dst.GetPixelCoordinate(offset + size, ImageSizeType::Relative), 0, 0, 1);
                break;
            }
            
            RenderCommand::CopyImage(frameContext.Cmd, srcCopy, dstCopy);
        });

    return pass;
}
