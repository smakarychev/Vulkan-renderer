#include "rendererpch.h"
#include "ClearTexturePass.h"

#include "FrameContext.h"
#include "RenderGraph/RGGraph.h"

Passes::ClearColorTexture::PassData& Passes::ClearColorTexture::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    
    struct PassDataPrivate : PassData
    {
        ColorClearValue ClearValue{};
    };

    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            passData.Image = graph.WriteImage(info.Image, ResourceAccessFlags::Transfer);
            passData.ClearValue = info.ClearValue;
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            GPU_PROFILE_FRAME("Texture.Clear.Color")

            frameContext.CommandList.ClearColorImage({
                .Image = {.Image = graph.GetImage(passData.Image)}, .ClearValue = passData.ClearValue
            });
        });
}

Passes::ClearDepthTexture::PassData& Passes::ClearDepthTexture::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    
    struct PassDataPrivate : PassData
    {
        DepthStencilClearValue ClearValue{};
    };

    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            passData.Image = graph.WriteImage(info.Image, ResourceAccessFlags::Transfer);
            passData.ClearValue = info.ClearValue;
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            GPU_PROFILE_FRAME("Texture.Clear.DepthStencil")

            frameContext.CommandList.ClearDepthStencilImage({
                .Image = {.Image = graph.GetImage(passData.Image)}, .ClearValue = passData.ClearValue
            });
        });
}
