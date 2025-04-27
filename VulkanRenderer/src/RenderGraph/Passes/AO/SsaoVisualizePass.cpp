#include "SsaoVisualizePass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SsaoVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::SsaoVisualize::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource ssao)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate
    {
        Resource SSAO{};
        Resource Color{};
    };
    
    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("SSAO.Visualize.Setup")

            graph.SetShader("ssao-visualize.shader");
            
            auto& ssaoDescription = Resources(graph).GetTextureDescription(ssao);
            passData.Color = graph.CreateResource("Color"_hsv,
                GraphTextureDescription{
                    .Width = ssaoDescription.Width,
                    .Height = ssaoDescription.Height,
                    .Format = Format::RGBA16_FLOAT});

            passData.SSAO = graph.Read(ssao, Pixel | Sampled);
            passData.Color = graph.RenderTarget(passData.Color, AttachmentLoad::Load, AttachmentStore::Store);

            PassData passDataPublic = {};
            passDataPublic.Color = passData.Color;

            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
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
}
