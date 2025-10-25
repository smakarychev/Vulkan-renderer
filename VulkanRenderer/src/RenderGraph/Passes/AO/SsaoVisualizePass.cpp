#include "rendererpch.h"

#include "SsaoVisualizePass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SsaoVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::SsaoVisualize::PassData& Passes::SsaoVisualize::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource ssao)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate : PassData
    {
        Resource SSAO{};
    };

    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("SSAO.Visualize.Setup")

            graph.SetShader("ssao-visualize"_hsv);
            
            passData.Color = graph.Create("Color"_hsv,
                RGImageDescription{
                    .Inference = RGImageInference::Size,
                    .Reference = ssao,
                    .Format = Format::RGBA16_FLOAT});

            passData.SSAO = graph.ReadImage(ssao, Pixel | Sampled);
            passData.Color = graph.RenderTarget(passData.Color, {});
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("SSAO.Visualize")
            GPU_PROFILE_FRAME("SSAO.Visualize")

            const Shader& shader = graph.GetShader();
            SsaoVisualizeShaderBindGroup bindGroup(shader);
            bindGroup.SetSsao(graph.GetImageBinding(passData.SSAO));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.Draw({.VertexCount = 3});
        });
}
