#include "BlitPass.h"

#include "Renderer.h"
#include "Vulkan/RenderCommand.h"

BlitPass::BlitPass(std::string_view name)
    : m_Name(name)
{
}

void BlitPass::AddToGraph(RG::Graph& renderGraph, RG::Resource textureIn,
    RG::Resource textureOut, const glm::vec3& offset, const glm::vec3& size, ImageSizeType sizeType)
{
    using namespace RG;
    
    m_Pass = &renderGraph.AddRenderPass<PassData>(m_Name,
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
            
            const Texture& src = resources.GetTexture(passData.TextureIn);
            const Texture& dst = resources.GetTexture(passData.TextureOut);

            RenderCommand::BlitImage(frameContext.Cmd,
                src.BlitInfo(),
                dst.BlitInfo(
                    offset, offset + size, 0, 0, 1, sizeType),
                ImageFilter::Nearest);
        });
}