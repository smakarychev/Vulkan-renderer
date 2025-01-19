#include "AtmosphereSkyViewLutPass.h"

#include "cvars/CVarSystem.h"
#include "Light/SceneLight.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Atmosphere::SkyView::addToGraph(std::string_view name, RG::Graph& renderGraph,
    RG::Resource transmittanceLut, RG::Resource multiscatteringLut,
    RG::Resource atmosphereSettings, const SceneLight& light)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.SkyView.Setup")

            graph.SetShader("atmosphere-sky-view-lut.shader");

            auto& globalResources = graph.GetGlobalResources();
            
            passData.Lut = graph.CreateResource(std::format("{}.Lut", name), GraphTextureDescription{
                .Width = (u32)*CVars::Get().GetI32CVar({"Atmosphere.SkyView.Width"}),
                .Height = (u32)*CVars::Get().GetI32CVar({"Atmosphere.SkyView.Height"}),
                .Format = Format::RGBA16_FLOAT});
            passData.DirectionalLight = graph.AddExternal(std::format("{}.DirectionalLight", name),
                light.GetBuffers().DirectionalLight);
            
            passData.AtmosphereSettings = graph.Read(atmosphereSettings, Compute | Uniform);
            passData.TransmittanceLut = graph.Read(transmittanceLut, Compute | Sampled);
            passData.MultiscatteringLut = graph.Read(multiscatteringLut, Compute | Sampled);
            passData.DirectionalLight = graph.Read(passData.DirectionalLight, Compute | Uniform);
            passData.Camera = graph.Read(globalResources.PrimaryCameraGPU, Compute | Uniform);
            passData.Lut = graph.Write(passData.Lut, Compute | Storage);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Atmosphere.SkyView")
            GPU_PROFILE_FRAME("Atmosphere.SkyView")

            const Shader& shader = resources.GetGraph()->GetShader();
            auto pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            const Texture& lutTexture = resources.GetTexture(passData.Lut);
            
            resourceDescriptors.UpdateBinding("u_atmosphere_settings",
                resources.GetBuffer(passData.AtmosphereSettings).BindingInfo());
            resourceDescriptors.UpdateBinding("u_directional_light",
                resources.GetBuffer(passData.DirectionalLight).BindingInfo());
            resourceDescriptors.UpdateBinding("u_camera",
                resources.GetBuffer(passData.Camera).BindingInfo());
            resourceDescriptors.UpdateBinding("u_transmittance_lut",
                resources.GetTexture(passData.TransmittanceLut).BindingInfo(
                    ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_multiscattering_lut",
                resources.GetTexture(passData.MultiscatteringLut).BindingInfo(
                    ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_sky_view_lut",
                lutTexture.BindingInfo(ImageFilter::Linear, ImageLayout::General));

            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindComputeImmutableSamplers(cmd, shader.GetLayout());
            RenderCommand::BindCompute(cmd, pipeline);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            RenderCommand::Dispatch(cmd,
                {lutTexture.Description().Width, lutTexture.Description().Height, 1},
                {16, 16, 1});
        });
}
