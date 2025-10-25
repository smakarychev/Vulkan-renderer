#include "rendererpch.h"

#include "FxaaPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/FxaaBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::Fxaa::PassData& Passes::Fxaa::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource colorIn)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Fxaa.Luminance.Setup")

            graph.SetShader("fxaa"_hsv);

            passData.AntiAliased = graph.Create("AntiAliased"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size,
                .Reference = colorIn,
                .Format = Format::RGBA16_FLOAT});
            
            passData.ColorIn = graph.ReadImage(colorIn, Compute | Sampled);
            passData.AntiAliased = graph.WriteImage(passData.AntiAliased, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Fxaa.Luminance")
            GPU_PROFILE_FRAME("Fxaa.Luminance")

            auto&& [input, inputDescription] = graph.GetImageWithDescription(passData.ColorIn);
            
            const Shader& shader = graph.GetShader();
            FxaaShaderBindGroup bindGroup(shader);
            bindGroup.SetColor(graph.GetImageBinding(passData.ColorIn));
            bindGroup.SetAntialiased(graph.GetImageBinding(passData.AntiAliased));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.Dispatch({
				.Invocations = {inputDescription.Width, inputDescription.Height, 1},
				.GroupSize = {16, 16, 1}});
        });
}
