#include "FxaaPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/FxaaBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::Fxaa::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource colorIn)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Fxaa.Luminance.Setup")

            graph.SetShader("fxaa"_hsv);

            auto& description = graph.GetTextureDescription(colorIn);
            passData.AntiAliased = graph.CreateResource("AntiAliased"_hsv, GraphTextureDescription{
                .Width = description.Width,
                .Height = description.Height,
                .Format = Format::RGBA16_FLOAT});
            
            passData.ColorIn = graph.Read(colorIn, Compute | Sampled);
            passData.AntiAliased = graph.Write(passData.AntiAliased, Compute | Storage);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Fxaa.Luminance")
            GPU_PROFILE_FRAME("Fxaa.Luminance")

            auto&& [input, inputDescription] = resources.GetTextureWithDescription(passData.ColorIn);
            
            const Shader& shader = resources.GetGraph()->GetShader();
            FxaaShaderBindGroup bindGroup(shader);
            bindGroup.SetColor({.Image = resources.GetTexture(passData.ColorIn)}, ImageLayout::Readonly);
            bindGroup.SetAntialiased({.Image = resources.GetTexture(passData.AntiAliased)}, ImageLayout::General);

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetFrameAllocators());
            cmd.Dispatch({
				.Invocations = {inputDescription.Width, inputDescription.Height, 1},
				.GroupSize = {16, 16, 1}});
        });
}
