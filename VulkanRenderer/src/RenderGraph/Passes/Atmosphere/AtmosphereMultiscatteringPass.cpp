#include "AtmosphereMultiscatteringPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

RG::Pass& Passes::Atmosphere::Multiscattering::addToGraph(std::string_view name, RG::Graph& renderGraph,
    RG::Resource transmittanceLut, RG::Resource atmosphereSettings)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Multiscattering.Setup")

            graph.SetShader("../assets/shaders/atmosphere-multiscattering-lut.shader");

            passData.Lut = graph.CreateResource(std::format("{}.Lut", name), GraphTextureDescription{
                .Width = (u32)*CVars::Get().GetI32CVar({"Atmosphere.Multiscattering.Size"}),
                .Height = (u32)*CVars::Get().GetI32CVar({"Atmosphere.Multiscattering.Size"}),
                .Format = Format::RGBA16_FLOAT});
            
            passData.AtmosphereSettings = graph.Read(atmosphereSettings, Compute | Uniform);
            passData.TransmittanceLut = graph.Read(transmittanceLut, Compute | Sampled);
            passData.Lut = graph.Write(passData.Lut, Compute | Storage);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Atmosphere.Multiscattering")
            GPU_PROFILE_FRAME("Atmosphere.Multiscattering")

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            const Texture& lutTexture = resources.GetTexture(passData.Lut);
            
            resourceDescriptors.UpdateBinding("u_atmosphere_settings",
                resources.GetBuffer(passData.AtmosphereSettings).BindingInfo());
            resourceDescriptors.UpdateBinding("u_transmittance_lut",
                resources.GetTexture(passData.TransmittanceLut).BindingInfo(
                    ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_multiscattering_lut",
                lutTexture.BindingInfo(ImageFilter::Linear, ImageLayout::General));

            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindComputeImmutableSamplers(cmd, shader.GetLayout());
            pipeline.BindCompute(cmd);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            RenderCommand::Dispatch(cmd,
                {lutTexture.Description().Width, lutTexture.Description().Height, 64},
                {1, 1, 64});
        });
}
