#include "rendererpch.h"

#include "DiffuseIrradiancePass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/DiffuseIrradianceBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::DiffuseIrradiance::PassData& Passes::DiffuseIrradiance::addToGraph(StringId name, RG::Graph& renderGraph,
    Texture cubemap, Texture irradiance)
{
    return addToGraph(name, renderGraph,
        renderGraph.Import("Cubemap"_hsv, cubemap, ImageLayout::Readonly),
        irradiance);
}

Passes::DiffuseIrradiance::PassData& Passes::DiffuseIrradiance::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource cubemap, Texture irradiance)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("DiffuseIrradiance.Setup")

            graph.SetShader("diffuse-irradiance"_hsv);

            passData.DiffuseIrradiance = graph.Import("DiffuseIrradiance"_hsv, irradiance);
                
            passData.DiffuseIrradiance = graph.WriteImage(passData.DiffuseIrradiance, Compute | Storage);
            passData.Cubemap = graph.ReadImage(cubemap, Compute | Sampled);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("DiffuseIrradiance")
            GPU_PROFILE_FRAME("DiffuseIrradiance")

            auto& irradianceDescription = graph.GetImageDescription(passData.DiffuseIrradiance);

            const Shader& shader = graph.GetShader();
            DiffuseIrradianceShaderBindGroup bindGroup(shader);

            bindGroup.SetEnv(graph.GetImageBinding(passData.Cubemap));
            bindGroup.SetIrradiance(graph.GetImageBinding(passData.DiffuseIrradiance));

            struct PushConstants
            {
                glm::vec2 DiffuseIrradianceResolutionInverse{};
            };
            PushConstants pushConstants = {
                .DiffuseIrradianceResolutionInverse = 1.0f / glm::vec2{(f32)irradianceDescription.Width}};

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {pushConstants}});
            cmd.Dispatch({
                .Invocations = {irradianceDescription.Width, irradianceDescription.Width, 6},
                .GroupSize = {32, 32, 1}});
        });
}
