#include "VisualizeBRDFPass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/BrdfVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::VisualizeBRDF::addToGraph(StringId name, RG::Graph& renderGraph, Texture brdf,
    RG::Resource colorIn, const glm::uvec2& resolution)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("BRDF.Visualize.Setup")

            graph.SetShader("brdf-visualize"_hsv);
            
            passData.ColorOut = RgUtils::ensureResource(colorIn, graph, "ColorOut"_hsv,
                GraphTextureDescription{
                    .Width = resolution.x,
                    .Height = resolution.y,
                    .Format = Format::RGBA16_FLOAT});

            passData.BRDF = graph.AddExternal("BRDF"_hsv, brdf);

            passData.BRDF = graph.Read(passData.BRDF, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                AttachmentLoad::Load, AttachmentStore::Store);
            
            Sampler brdfSampler = Device::CreateSampler({
                .WrapMode = SamplerWrapMode::ClampBorder});

            passData.BRDFSampler = brdfSampler;

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {   
            CPU_PROFILE_FRAME("BRDF.Visualize")
            GPU_PROFILE_FRAME("BRDF.Visualize")

            const Shader& shader = resources.GetGraph()->GetShader();
            BrdfVisualizeShaderBindGroup bindGroup(shader);

            bindGroup.SetSampler(passData.BRDFSampler);
            bindGroup.SetBrdf({.Image = brdf}, ImageLayout::Readonly);

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetFrameAllocators());
            cmd.Draw({.VertexCount = 3});
        });

    return pass;
}
