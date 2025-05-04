#include "AtmospherePass.h"

#include "AtmosphereAerialPerspectiveLutPass.h"
#include "AtmosphereMultiscatteringPass.h"
#include "AtmosphereRaymarchPass.h"
#include "AtmosphereSkyViewLutPass.h"
#include "AtmosphereTransmittanceLutPass.h"
#include "Environment/AtmosphereEnvironmentPass.h"
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

Passes::Atmosphere::PassData& Passes::Atmosphere::addToGraph(StringId name, RG::Graph& renderGraph,
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

            passData.AtmosphereSettings = graph.CreateResource("Settings"_hsv, GraphBufferDescription{
                .SizeBytes = sizeof(AtmosphereSettings)});
            graph.Upload(passData.AtmosphereSettings, atmosphereSettings);

            auto& transmittance = Transmittance::addToGraph("Transmittance"_hsv, graph,
                passData.AtmosphereSettings);
            
            auto& multiscattering = Multiscattering::addToGraph("Multiscattering"_hsv, graph,
                transmittance.Lut, passData.AtmosphereSettings);

            auto& skyView = SkyView::addToGraph("SkyView"_hsv, graph,
                transmittance.Lut, multiscattering.Lut, passData.AtmosphereSettings, light);

            auto& aerialPerspective = AerialPerspective::addToGraph("AerialPerspective"_hsv, graph,
                multiscattering.TransmittanceLut, multiscattering.Lut, passData.AtmosphereSettings, light,
                csmData);

            static constexpr bool USE_SUN_LUMINANCE = true;
            auto& atmosphere = Raymarch::addToGraph("Raymarch"_hsv, graph,
                passData.AtmosphereSettings, *globalResources.PrimaryCamera, light,
                skyView.Lut, multiscattering.TransmittanceLut, aerialPerspective.Lut,
                colorIn, {}, depthIn, USE_SUN_LUMINANCE);
            
            auto& environment = Environment::addToGraph("Environment"_hsv, graph,
                    passData.AtmosphereSettings, light, skyView.Lut);

            passData.TransmittanceLut = transmittance.Lut;
            passData.MultiscatteringLut = multiscattering.Lut;
            passData.SkyViewLut = skyView.Lut;
            passData.AerialPerspectiveLut = aerialPerspective.Lut;
            passData.Atmosphere = atmosphere.ColorOut;
            passData.AtmosphereSettings = atmosphere.AtmosphereSettings;
            passData.EnvironmentOut = environment.ColorOut;
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        }).Data;
}
