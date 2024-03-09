#include "BlitPass.h"

#include "Renderer.h"
#include "Vulkan/RenderCommand.h"

BlitPass::BlitPass(RenderGraph::Graph& renderGraph, RenderGraph::Resource textureIn, RenderGraph::Resource colorTarget)
{
    using namespace RenderGraph;
    
    m_Pass = &renderGraph.AddRenderPass<PassData>("blit-pass",
        [&](Graph& graph, PassData& passData)
        {
            passData.TextureIn = graph.Read(textureIn,
                ResourceAccessFlags::Blit);

            passData.TextureOut = graph.Write(colorTarget,
                ResourceAccessFlags::Blit);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Texture blit")
            
            const Texture& src = resources.GetTexture(passData.TextureIn);
            const Texture& dst = resources.GetTexture(passData.TextureOut);

            RenderCommand::BlitImage(frameContext.Cmd, src.CreateImageBlitInfo(), dst.CreateImageBlitInfo(),
                ImageFilter::Nearest);
        });
}
