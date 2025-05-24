#include "VisualizeBRDFPass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/BrdfVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::VisualizeBRDF::PassData& Passes::VisualizeBRDF::addToGraph(StringId name, RG::Graph& renderGraph, Texture brdf,
    RG::Resource colorIn, const glm::uvec2& resolution)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("BRDF.Visualize.Setup")

            graph.SetShader("brdf-visualize"_hsv);
            
            passData.ColorOut = RgUtils::ensureResource(colorIn, graph, "ColorOut"_hsv,
                RGImageDescription{
                    .Width = (f32)resolution.x,
                    .Height = (f32)resolution.y,
                    .Format = Format::RGBA16_FLOAT});

            passData.BRDF = graph.Import("BRDF"_hsv, brdf, ImageLayout::Readonly);

            passData.BRDF = graph.ReadImage(passData.BRDF, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, {});
            
            Sampler brdfSampler = Device::CreateSampler({
                .WrapMode = SamplerWrapMode::ClampBorder});

            passData.BRDFSampler = brdfSampler;
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {   
            CPU_PROFILE_FRAME("BRDF.Visualize")
            GPU_PROFILE_FRAME("BRDF.Visualize")

            const Shader& shader = graph.GetShader();
            BrdfVisualizeShaderBindGroup bindGroup(shader);

            bindGroup.SetSampler(passData.BRDFSampler);
            bindGroup.SetBrdf(graph.GetImageBinding(passData.BRDF));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.Draw({.VertexCount = 3});
        });
}
