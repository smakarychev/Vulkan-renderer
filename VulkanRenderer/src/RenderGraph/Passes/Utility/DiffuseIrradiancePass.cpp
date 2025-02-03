#include "DiffuseIrradiancePass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/DiffuseIrradianceBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"


RG::Pass& Passes::DiffuseIrradiance::addToGraph(std::string_view name, RG::Graph& renderGraph, Texture cubemap,
    Texture irradiance)
{
    return addToGraph(name, renderGraph,
        renderGraph.AddExternal(std::format("{}.Cubemap", name), cubemap),
        irradiance);
}

RG::Pass& Passes::DiffuseIrradiance::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource cubemap,
    Texture irradiance)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("DiffuseIrradiance.Setup")

            graph.SetShader("diffuse-irradiance.shader");

            passData.DiffuseIrradiance = graph.AddExternal(std::format("{}.DiffuseIrradiance", name), irradiance);
                
            passData.DiffuseIrradiance = graph.Write(passData.DiffuseIrradiance, Compute | Storage);
            passData.Cubemap = graph.Read(cubemap, Compute | Sampled);
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("DiffuseIrradiance")
            GPU_PROFILE_FRAME("DiffuseIrradiance")

            Texture cubemapTexture = resources.GetTexture(passData.Cubemap);
            auto&& [irradianceTexture, irradianceDescription] =
                resources.GetTextureWithDescription(passData.DiffuseIrradiance);

            const Shader& shader = resources.GetGraph()->GetShader();
            DiffuseIrradianceShaderBindGroup bindGroup(shader);

            bindGroup.SetEnv({.Image = cubemapTexture}, ImageLayout::Readonly);
            bindGroup.SetIrradiance({.Image = irradianceTexture}, ImageLayout::General);

            struct PushConstants
            {
                glm::vec2 DiffuseIrradianceResolutionInverse{};
            };
            PushConstants pushConstants = {
                .DiffuseIrradianceResolutionInverse = 1.0f / glm::vec2{(f32)irradianceDescription.Width}};

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {pushConstants}});
            cmd.Dispatch({
                .Invocations = {irradianceDescription.Width, irradianceDescription.Width, 6},
                .GroupSize = {32, 32, 1}});
        });
}
