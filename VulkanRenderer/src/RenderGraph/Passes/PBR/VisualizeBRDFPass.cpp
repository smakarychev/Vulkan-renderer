#include "VisualizeBRDFPass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::VisualizeBRDF::addToGraph(std::string_view name, RG::Graph& renderGraph, const Texture& brdf,
    RG::Resource colorIn, const glm::uvec2& resolution)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(PassName{"BRDF.Visualize"},
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("BRDF.Visualize.Setup");

            graph.SetShader("../assets/shaders/brdf-visualize.shader");
            
            passData.ColorOut = RG::RgUtils::ensureResource(colorIn, graph, "BRDF.Visualize.ColorOut",
                GraphTextureDescription{
                    .Width = resolution.x,
                    .Height = resolution.y,
                    .Format = Format::RGBA16_FLOAT});

            passData.BRDF = graph.AddExternal("BRDF.Visualize.BRDF", brdf);

            passData.BRDF = graph.Read(passData.BRDF, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                AttachmentLoad::Load, AttachmentStore::Store);
            
            Sampler brdfSampler = Sampler::Builder()
                .Filters(ImageFilter::Linear, ImageFilter::Linear)
                .WrapMode(SamplerWrapMode::ClampBorder)
                .Build();

            passData.BRDFSampler = brdfSampler;

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {   
            CPU_PROFILE_FRAME("BRDF.Visualize");
            GPU_PROFILE_FRAME("BRDF.Visualize");

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            samplerDescriptors.UpdateBinding("u_sampler", brdf.BindingInfo(passData.BRDFSampler,
                ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding(UNIFORM_BRDF, brdf.BindingInfo(passData.BRDFSampler, ImageLayout::Readonly));

            auto& cmd = frameContext.Cmd;
            pipeline.BindGraphics(cmd);
            samplerDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            RenderCommand::Draw(cmd, 3);
        });

    return pass;
}
