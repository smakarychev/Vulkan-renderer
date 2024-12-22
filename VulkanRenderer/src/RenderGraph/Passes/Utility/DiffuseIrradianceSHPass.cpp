#include "DiffuseIrradianceSHPass.h"

#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::DiffuseIrradianceSH::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const Texture& cubemap, const Buffer& irradianceSH, bool realTime)
{
    return addToGraph(name, renderGraph,
        renderGraph.AddExternal(std::format("{}.CubemapTexture", name), cubemap),
        irradianceSH, realTime);
}

RG::Pass& Passes::DiffuseIrradianceSH::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource cubemap,
    const Buffer& irradianceSH, bool realTime)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("DiffuseIrradianceSH.Setup")

            graph.SetShader("../assets/shaders/diffuse-irradiance-sh.shader",
                ShaderOverrides{}
                    .Add({"REAL_TIME"}, realTime));
            
            passData.DiffuseIrradiance = graph.AddExternal(std::format("{}.DiffuseIrradianceSH", name), irradianceSH);
            
            passData.DiffuseIrradiance = graph.Write(passData.DiffuseIrradiance, Compute | Storage);
            passData.CubemapTexture = graph.Read(cubemap, Compute | Sampled);
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("DiffuseIrradianceSH")
            GPU_PROFILE_FRAME("DiffuseIrradianceSH")

            const Buffer& diffuseIrradiance = resources.GetBuffer(passData.DiffuseIrradiance);
            const Texture& cubemapTexture = resources.GetTexture(passData.CubemapTexture);

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_sh", diffuseIrradiance.BindingInfo());
            resourceDescriptors.UpdateBinding("u_env", cubemapTexture.BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));

            const u32 realTimeMipmapsCount = (u32)std::log(16.0);
            const u32 targetMipmap = realTime ?
                std::min(cubemapTexture.Description().Mipmaps, (i8)realTimeMipmapsCount) - realTimeMipmapsCount :
                0;
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindComputeImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), targetMipmap);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());

            RenderCommand::Dispatch(cmd, {1, 1, 1});
        });

    return pass;
}
