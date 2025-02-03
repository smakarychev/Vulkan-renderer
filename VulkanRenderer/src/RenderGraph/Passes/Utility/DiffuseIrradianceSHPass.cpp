#include "DiffuseIrradianceSHPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/DiffuseIrradianceShBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::DiffuseIrradianceSH::addToGraph(std::string_view name, RG::Graph& renderGraph,
    Texture cubemap, Buffer irradianceSH, bool realTime)
{
    return addToGraph(name, renderGraph,
        renderGraph.AddExternal(std::format("{}.CubemapTexture", name), cubemap),
        irradianceSH, realTime);
}

RG::Pass& Passes::DiffuseIrradianceSH::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource cubemap,
    Buffer irradianceSH, bool realTime)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("DiffuseIrradianceSH.Setup")

            graph.SetShader("diffuse-irradiance-sh.shader",
                ShaderOverrides{
                    ShaderOverride{{"REAL_TIME"}, realTime}});
            
            passData.DiffuseIrradiance = graph.AddExternal(std::format("{}.DiffuseIrradianceSH", name), irradianceSH);
            
            passData.DiffuseIrradiance = graph.Write(passData.DiffuseIrradiance, Compute | Storage);
            passData.CubemapTexture = graph.Read(cubemap, Compute | Sampled);
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("DiffuseIrradianceSH")
            GPU_PROFILE_FRAME("DiffuseIrradianceSH")

            Buffer diffuseIrradiance = resources.GetBuffer(passData.DiffuseIrradiance);
            auto&& [cubemapTexture, cubemapDescription] = resources.GetTextureWithDescription(passData.CubemapTexture);

            const Shader& shader = resources.GetGraph()->GetShader();
            DiffuseIrradianceShShaderBindGroup bindGroup(shader);

            bindGroup.SetSh({.Buffer = diffuseIrradiance});
            bindGroup.SetEnv({.Image = cubemapTexture}, ImageLayout::Readonly);

            const u32 realTimeMipmapsCount = (u32)std::log(16.0);
            const u32 targetMipmap = realTime ?
                std::min(cubemapDescription.Mipmaps, (i8)realTimeMipmapsCount) - realTimeMipmapsCount :
                0;
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {targetMipmap}});
            cmd.Dispatch({
                .Invocations = {1, 1, 1}});
        });

    return pass;
}
