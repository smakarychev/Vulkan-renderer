#include "FxaaPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/FxaaBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Fxaa::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource colorIn)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Fxaa.Luminance.Setup")

            graph.SetShader("fxaa.shader");

            auto& description = graph.GetTextureDescription(colorIn);
            passData.AntiAliased = graph.CreateResource(std::format("{}.AntiAliased", name), GraphTextureDescription{
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

            auto& cmd = frameContext.Cmd;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            RenderCommand::Dispatch(cmd,
                {inputDescription.Width, inputDescription.Height, 1},
                {16, 16, 1});
        });
}
