#include "DiffuseIrradiancePass.h"

#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"


RG::Pass& Passes::DiffuseIrradiance::addToGraph(std::string_view name, RG::Graph& renderGraph, const Texture& cubemap,
    const Texture& irradiance)
{
    return addToGraph(name, renderGraph,
        renderGraph.AddExternal(std::format("{}.Cubemap", name), cubemap),
        irradiance);
}

RG::Pass& Passes::DiffuseIrradiance::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource cubemap,
    const Texture& irradiance)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("DiffuseIrradiance.Setup")

            graph.SetShader("../assets/shaders/diffuse-irradiance.shader");

            passData.DiffuseIrradiance = graph.AddExternal(std::format("{}.DiffuseIrradiance", name), irradiance);
                
            passData.DiffuseIrradiance = graph.Write(passData.DiffuseIrradiance, Compute | Storage);
            passData.Cubemap = graph.Read(cubemap, Compute | Sampled);
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("DiffuseIrradiance")
            GPU_PROFILE_FRAME("DiffuseIrradiance")

            const Texture& cubemapTexture = resources.GetTexture(passData.Cubemap);
            const Texture& diffuseIrradianceTexture = resources.GetTexture(passData.DiffuseIrradiance);

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_env", cubemapTexture.BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_irradiance", diffuseIrradianceTexture.BindingInfo(
                ImageFilter::Linear, ImageLayout::General));

            struct PushConstants
            {
                glm::vec2 DiffuseIrradianceResolutionInverse{};
            };
            PushConstants pushConstants = {
                .DiffuseIrradianceResolutionInverse =
                    1.0f / glm::vec2{(f32)diffuseIrradianceTexture.Description().Width}};

            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindComputeImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());

            RenderCommand::Dispatch(cmd,
                {diffuseIrradianceTexture.Description().Width, diffuseIrradianceTexture.Description().Width, 6},
                {32, 32, 1});
        });
}
