#include "AtmospherePass.h"

#include "AtmosphereMultiscatteringPass.h"
#include "AtmosphereSkyViewLutPass.h"
#include "AtmosphereTransmittanceLutPass.h"
#include "RenderGraph/RGGraph.h"
#include "Rendering/Shader/ShaderCache.h"

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

Passes::Atmosphere::LutPasses::PassData& Passes::Atmosphere::LutPasses::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Lut.Setup")
            
            passData.AtmosphereSettings = graph.Create("Settings"_hsv, RGBufferDescription{
                .SizeBytes = sizeof(AtmosphereSettings)});
            graph.Upload(passData.AtmosphereSettings, *info.AtmosphereSettings);

            auto& transmittance = Transmittance::addToGraph("Transmittance"_hsv, graph,
                passData.AtmosphereSettings);
            
            auto& multiscattering = Multiscattering::addToGraph("Multiscattering"_hsv, graph,
                transmittance.Lut, passData.AtmosphereSettings);

            auto& skyView = SkyView::addToGraph("SkyView"_hsv, graph,
                transmittance.Lut, multiscattering.Lut, passData.AtmosphereSettings, *info.SceneLight);

            passData.TransmittanceLut = transmittance.Lut;
            passData.MultiscatteringLut = multiscattering.Lut;
            passData.SkyViewLut = skyView.Lut;
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}