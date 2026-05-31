#include "rendererpch.h"

#include "AtmospherePass.h"

#include "AtmosphereMultiscatteringPass.h"
#include "AtmosphereSkyViewLutPass.h"
#include "AtmosphereTransmittanceAtViewPass.h"
#include "AtmosphereTransmittanceLutPass.h"
#include "RenderGraph/RGGraph.h"

Passes::Atmosphere::LutPasses::PassData& Passes::Atmosphere::LutPasses::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Lut.Setup")
            
            auto& transmittance = Transmittance::addToGraph("Transmittance"_hsv, graph, {
                .ViewInfo = info.ViewInfo
            });
            
            auto& updatedViewInfo = AtmosphereUpdateSunParameters::addToGraph("AtmosphereUpdateSunParameters"_hsv,
                graph, {
                    .ViewInfo = info.ViewInfo,
                    .TransmittanceLut = transmittance.Lut
                }).ViewInfo;
            
            auto& multiscattering = Multiscattering::addToGraph("Multiscattering"_hsv, graph, {
                .ViewInfo = updatedViewInfo,
                .TransmittanceLut = transmittance.Lut,
            });

            auto& skyView = SkyView::addToGraph("SkyView"_hsv, graph, {
                .ViewInfo = updatedViewInfo,
                .TransmittanceLut = transmittance.Lut,
                .MultiscatteringLut = multiscattering.Lut,
            });

            passData.TransmittanceLut = transmittance.Lut;
            passData.MultiscatteringLut = multiscattering.Lut;
            passData.SkyViewLut = skyView.Lut;
            passData.ViewInfo = updatedViewInfo;
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}