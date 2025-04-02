#include "SsaoVisualizePass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SsaoVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::SsaoVisualize::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource ssao,
    RG::Resource colorOut)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("SSAO.Visualize.Setup")

            graph.SetShader("ssao-visualize.shader");
            
            auto& ssaoDescription = Resources(graph).GetTextureDescription(ssao);
            passData.ColorOut = RgUtils::ensureResource(colorOut, graph, "Color"_hsv,
                GraphTextureDescription{
                    .Width = ssaoDescription.Width,
                    .Height = ssaoDescription.Height,
                    .Format = Format::RGBA16_FLOAT});

            passData.SSAO = graph.Read(ssao, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("SSAO.Visualize")
            GPU_PROFILE_FRAME("SSAO.Visualize")

            Texture ssaoTexture = resources.GetTexture(passData.SSAO);

            const Shader& shader = resources.GetGraph()->GetShader();
            SsaoVisualizeShaderBindGroup bindGroup(shader);
            bindGroup.SetSsao({.Image = ssaoTexture}, ImageLayout::Readonly);

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            cmd.Draw({.VertexCount = 3});
        });

    return pass;
}
