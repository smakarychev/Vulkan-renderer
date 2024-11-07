#include "AtmospherePass.h"

#include "AtmosphereAerialPerspectiveLutPass.h"
#include "AtmosphereMultiscatteringPass.h"
#include "AtmosphereSkyViewLutPass.h"
#include "AtmosphereTransmittanceLutPass.h"
#include "Light/SceneLight.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

AtmosphereSettings AtmosphereSettings::EarthDefault()
{
    return {
        .RayleighScattering = glm::vec4{0.005802f, 0.013558f, 0.0331f, 1.0f},
        .RayleighAbsorption = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f},
        .MieScattering = glm::vec4{0.003996f, 0.003996f, 0.003996f, 1.0f},
        .MieAbsorption = glm::vec4{0.0044f, 0.0044f, 0.0044f, 1.0f},
        .OzoneAbsorption = glm::vec4{0.000650f, 0.001881f, 0.000085f, 1.0f},
        .SurfaceAlbedo = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f},
        .Surface = 6360.0f,
        .Atmosphere = 6460.0f,
        .RayleighDensity = 1.0f,
        .MieDensity = 1.0f,
        .OzoneDensity = 1.0f};
}

namespace
{
    struct RayMarchPassData
    {
        RG::Resource DepthIn{};
        RG::Resource AtmosphereSettings{};
        RG::Resource SkyViewLut{};
        RG::Resource TransmittanceLut{};
        RG::Resource AerialPerspectiveLut{};
        RG::Resource Camera{};
        RG::Resource DirectionalLight{};
        RG::Resource ColorOut{};
    };
    RG::Pass& raymarchAtmospherePass(std::string_view name, RG::Graph& renderGraph,
        RG::Resource atmosphereSettings, const SceneLight& light,
        RG::Resource skyViewLut, RG::Resource transmittanceLut, RG::Resource aerialPerspectiveLut,
        RG::Resource colorIn, RG::Resource depthIn)
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
            passData.ColorOut = RgUtils::ensureResource(colorIn, graph, std::format("{}.ColorOut", name),
                GraphTextureDescription{
                    .Width = globalResources.Resolution.x,
                    .Height = globalResources.Resolution.y,
                    .Format = Format::RGBA16_FLOAT});

            if (depthIn.IsValid())
                passData.DepthIn = graph.Read(depthIn, Pixel | Sampled);

            passData.SkyViewLut = graph.Read(skyViewLut, Pixel | Sampled);
            passData.TransmittanceLut = graph.Read(transmittanceLut, Pixel | Sampled);
            passData.AerialPerspectiveLut = graph.Read(aerialPerspectiveLut, Pixel | Sampled);
            passData.AtmosphereSettings = graph.Read(atmosphereSettings, Pixel | Uniform);
            passData.DirectionalLight = graph.Read(passData.DirectionalLight, Pixel | Uniform);
            passData.Camera = graph.Read(globalResources.PrimaryCameraGPU, Pixel | Uniform);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);

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

            if (passData.DepthIn.IsValid())
                resourceDescriptors.UpdateBinding("u_depth", resources.GetTexture(passData.DepthIn).BindingInfo(
                    ImageFilter::Linear, ImageLayout::DepthReadonly));
            
            resourceDescriptors.UpdateBinding("u_atmosphere_settings",
                resources.GetBuffer(passData.AtmosphereSettings).BindingInfo());
            resourceDescriptors.UpdateBinding("u_directional_light",
                resources.GetBuffer(passData.DirectionalLight).BindingInfo());
            resourceDescriptors.UpdateBinding("u_camera",
                resources.GetBuffer(passData.Camera).BindingInfo());
            resourceDescriptors.UpdateBinding("u_sky_view_lut",
                resources.GetTexture(passData.SkyViewLut).BindingInfo(
                   ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_transmittance_lut",
                resources.GetTexture(passData.TransmittanceLut).BindingInfo(
                   ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_aerial_perspective_lut",
                resources.GetTexture(passData.AerialPerspectiveLut).BindingInfo(
                   ImageFilter::Linear, ImageLayout::Readonly));

            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindGraphics(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), passData.DepthIn.IsValid());
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Draw(cmd, 3);
        });
    }
}

RG::Pass& Passes::Atmosphere::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const AtmosphereSettings& atmosphereSettings, const SceneLight& light, RG::Resource colorIn, RG::Resource depthIn,
    const RG::CSMData& csmData)
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

            auto& transmittance = Transmittance::addToGraph(std::format("{}.Transmittance", name), graph,
                passData.AtmosphereSettings);
            auto& transmittanceOutput = graph.GetBlackboard().Get<Transmittance::PassData>(transmittance);
            
            auto& multiscattering = Multiscattering::addToGraph(std::format("{}.Multiscattering", name), graph,
                transmittanceOutput.Lut, passData.AtmosphereSettings);
            auto& multiscatteringOutput = graph.GetBlackboard().Get<Multiscattering::PassData>(multiscattering);

            auto& skyView = SkyView::addToGraph(std::format("{}.SkyView", name), graph,
                transmittanceOutput.Lut, multiscatteringOutput.Lut, passData.AtmosphereSettings, light);
            auto& skyViewOutput = graph.GetBlackboard().Get<SkyView::PassData>(skyView);

            auto& aerialPerspective = AerialPerspective::addToGraph(std::format("{}.AerialPerspective", name), graph,
                multiscatteringOutput.TransmittanceLut, multiscatteringOutput.Lut, passData.AtmosphereSettings, light,
                csmData);
            auto& aerialPerspectiveOutput = graph.GetBlackboard().Get<AerialPerspective::PassData>(aerialPerspective);

            auto& atmosphere = raymarchAtmospherePass(std::format("{}.Raymarch", name), graph,
                passData.AtmosphereSettings, light,
                skyViewOutput.Lut, multiscatteringOutput.TransmittanceLut, aerialPerspectiveOutput.Lut,
                colorIn, depthIn);
            auto& atmosphereOutput = graph.GetBlackboard().Get<RayMarchPassData>(atmosphere);
            passData.TransmittanceLut = transmittanceOutput.Lut;
            passData.MultiscatteringLut = multiscatteringOutput.Lut;
            passData.SkyViewLut = skyViewOutput.Lut;
            passData.AerialPerspectiveLut = aerialPerspectiveOutput.Lut;
            passData.ColorOut = atmosphereOutput.ColorOut;
            passData.AtmosphereSettings = atmosphereOutput.AtmosphereSettings;

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });
}
