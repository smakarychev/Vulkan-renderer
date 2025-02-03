#include "AtmospherePass.h"

#include "AtmosphereAerialPerspectiveLutPass.h"
#include "AtmosphereMultiscatteringPass.h"
#include "AtmosphereRaymarchPass.h"
#include "AtmosphereSkyViewLutPass.h"
#include "AtmosphereTransmittanceLutPass.h"
#include "Environment/AtmosphereEnvironmentPass.h"
#include "Light/SceneLight.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/Shader/ShaderCache.h"

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
            
            auto& globalResources = graph.GetGlobalResources();

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

            static constexpr bool USE_SUN_LUMINANCE = true;
            auto& atmosphere = Raymarch::addToGraph(std::format("{}.Raymarch", name), graph,
                passData.AtmosphereSettings, *globalResources.PrimaryCamera, light,
                skyViewOutput.Lut, multiscatteringOutput.TransmittanceLut, aerialPerspectiveOutput.Lut,
                colorIn, {}, depthIn, USE_SUN_LUMINANCE);
            auto& atmosphereOutput = graph.GetBlackboard().Get<Raymarch::PassData>(atmosphere);
            
            auto& environment = Environment::addToGraph(std::format("{}.Environment", name), graph,
                    passData.AtmosphereSettings, light, skyViewOutput.Lut);
            auto& environmentOutput = graph.GetBlackboard().Get<Environment::PassData>(environment);


            passData.TransmittanceLut = transmittanceOutput.Lut;
            passData.MultiscatteringLut = multiscatteringOutput.Lut;
            passData.SkyViewLut = skyViewOutput.Lut;
            passData.AerialPerspectiveLut = aerialPerspectiveOutput.Lut;
            passData.Atmosphere = atmosphereOutput.ColorOut;
            passData.AtmosphereSettings = atmosphereOutput.AtmosphereSettings;
            passData.EnvironmentOut = environmentOutput.ColorOut;

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });
}
