#include "CopyTexturePass.h"

#include "FrameContext.h"
#include "Vulkan/RenderCommand.h"

CopyTexturePass::CopyTexturePass(std::string_view name)
    : m_Name(name)
{
}

void CopyTexturePass::AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource textureIn,
    RenderGraph::Resource textureOut, const glm::vec3& offset, const glm::vec3& size, ImageSizeType sizeType)
{
    using namespace RenderGraph;
    
    m_Pass = &renderGraph.AddRenderPass<PassData>(m_Name,
        [&](Graph& graph, PassData& passData)
        {
            passData.TextureIn = graph.Read(textureIn,
                ResourceAccessFlags::Copy);

            passData.TextureOut = graph.Write(textureOut,
                ResourceAccessFlags::Copy);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Texture copy")
            
            const Texture& src = resources.GetTexture(passData.TextureIn);
            const Texture& dst = resources.GetTexture(passData.TextureOut);

            RenderCommand::CopyImage(frameContext.Cmd,
                src.CreateImageCopyInfo(),
                dst.CreateImageCopyInfo(
                    offset, offset + size, 0, 0, 1, sizeType));
        });
}
