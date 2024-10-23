#include "AtmospherePass.h"

#include "AtmosphereMultiscatteringPass.h"
#include "AtmosphereSkyViewLutPass.h"
#include "AtmosphereTransmittanceLutPass.h"
#include "Light/SceneLight.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

AtmosphereSettings AtmosphereSettings::EarthDefault()
{
    return {
        .RayleighScattering = glm::vec4{5.802f, 13.558f, 33.1f, 1.0f},
        .RayleighAbsorption = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f},
        .MieScattering = glm::vec4{3.996f, 3.996f, 3.996f, 1.0f},
        .MieAbsorption = glm::vec4{4.4f, 4.4f, 4.4f, 1.0f},
        .OzoneAbsorption = glm::vec4{0.650f, 1.881f, 0.085f, 1.0f},
        .SurfaceAlbedo = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f},
        .Surface = 6.360f,
        .Atmosphere = 6.460f,
        .RayleighDensity = 1.0f,
        .MieDensity = 1.0f,
        .OzoneDensity = 1.0f};
}

namespace
{
    struct RayMarchPassData
    {
        RG::Resource AtmosphereSettings{};
        RG::Resource SkyViewLut{};
        RG::Resource Camera{};
        RG::Resource DirectionalLight{};
        RG::Resource ColorOut{};
    };
    RG::Pass& raymarchAtmospherePass(std::string_view name, RG::Graph& renderGraph,
        RG::Resource atmosphereSettings, const SceneLight& light, RG::Resource skyViewLut)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        return renderGraph.AddRenderPass<RayMarchPassData>(name,
        [&](Graph& graph, RayMarchPassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Raymarch.Setup")

            graph.SetShader("../assets/shaders/atmosphere-raymarch.shader");

            passData.DirectionalLight = graph.AddExternal(std::format("{}.DirectionalLight", name),
                light.GetBuffers().DirectionalLight);
            auto& globalResources = graph.GetGlobalResources();
            passData.ColorOut = graph.CreateResource(std::format("{}.ColorOut", name), GraphTextureDescription{
                .Width = globalResources.Resolution.x,
                .Height = globalResources.Resolution.y,
                .Format = Format::RGBA16_FLOAT});

            passData.SkyViewLut = graph.Read(skyViewLut, Compute | Sampled);
            passData.AtmosphereSettings = graph.Read(atmosphereSettings, Compute | Uniform);
            passData.DirectionalLight = graph.Read(passData.DirectionalLight, Compute | Uniform);
            passData.Camera = graph.Read(globalResources.PrimaryCameraGPU, Compute | Uniform);
            passData.ColorOut = graph.Write(passData.ColorOut, Compute | Storage);

            graph.UpdateBlackboard(passData);
        },
        [=](RayMarchPassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Atmosphere.Raymarch")
            GPU_PROFILE_FRAME("Atmosphere.Raymarch")

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            const Texture& atmosphere = resources.GetTexture(passData.ColorOut);

            resourceDescriptors.UpdateBinding("u_atmosphere_settings",
                resources.GetBuffer(passData.AtmosphereSettings).BindingInfo());
            resourceDescriptors.UpdateBinding("u_directional_light",
                resources.GetBuffer(passData.DirectionalLight).BindingInfo());
            resourceDescriptors.UpdateBinding("u_camera",
                resources.GetBuffer(passData.Camera).BindingInfo());
            resourceDescriptors.UpdateBinding("u_sky_view_lut",
                resources.GetTexture(passData.SkyViewLut).BindingInfo(
                   ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_atmosphere",
                atmosphere.BindingInfo(ImageFilter::Linear, ImageLayout::General));

            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindComputeImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), glm::vec2{frameContext.Resolution});
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd,
                {atmosphere.Description().Width, atmosphere.Description().Height, 1},
                {16, 16, 1});
        });
    }
}

RG::Pass& Passes::Atmosphere::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const AtmosphereSettings& atmosphereSettings, const SceneLight& light)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Setup")

            passData.AtmosphereSettings = graph.CreateResource(std::format("{}.Settings", name), GraphBufferDescription{
                .SizeBytes = sizeof(AtmosphereSettings)});
            graph.Upload(passData.AtmosphereSettings, atmosphereSettings);

            auto& globalResources = graph.GetGlobalResources();
            passData.ColorOut = graph.CreateResource(std::format("{}.ColorOut", name), GraphTextureDescription{
                .Width = globalResources.Resolution.x,
                .Height = globalResources.Resolution.y,
                .Format = Format::RGBA16_FLOAT});

            auto& transmittance = TransmittanceLut::addToGraph(std::format("{}.Transmittance", name), graph,
                passData.AtmosphereSettings);
            auto& transmittanceOutput = graph.GetBlackboard().Get<TransmittanceLut::PassData>(transmittance);
            
            auto& multiscattering = Multiscattering::addToGraph(std::format("{}.Multiscattering", name), graph,
                transmittanceOutput.Lut, passData.AtmosphereSettings);
            auto& multiscatteringOutput = graph.GetBlackboard().Get<Multiscattering::PassData>(multiscattering);

            auto& skyView = SkyView::addToGraph(std::format("{}.SkyView", name), graph,
                transmittanceOutput.Lut, multiscatteringOutput.Lut, passData.AtmosphereSettings, light);
            auto& skyViewOutput = graph.GetBlackboard().Get<SkyView::PassData>(skyView);

            auto& atmosphere = raymarchAtmospherePass(std::format("{}.Raymarch", name), graph,
                passData.AtmosphereSettings, light, skyViewOutput.Lut);
            auto& atmosphereOutput = graph.GetBlackboard().Get<RayMarchPassData>(atmosphere);
            passData.TransmittanceLut = transmittanceOutput.Lut;
            passData.MultiscatteringLut = multiscatteringOutput.Lut;
            passData.SkyViewLut = atmosphereOutput.SkyViewLut;
            passData.ColorOut = atmosphereOutput.ColorOut;
            passData.AtmosphereSettings = atmosphereOutput.AtmosphereSettings;

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });
}
