#include "rendererpch.h"

#include "AtmosphereAerialPerspectiveLutPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/Passes/Generated/AtmosphereLutAerialPerspectiveBindGroupRG.generated.h"

Passes::Atmosphere::AerialPerspective::PassData& Passes::Atmosphere::AerialPerspective::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, AtmosphereLutAerialPerspectiveBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.AerialPerspective.Setup")

            passData.BindGroup = AtmosphereLutAerialPerspectiveBindGroupRG(graph, ShaderDefines({
                ShaderDefine{"PCF_FILTER_SIZE"_hsv, "2"}
            }));

            passData.Lut = passData.BindGroup.SetResourcesAerialPerspectiveLut(graph.Create("Lut"_hsv,
                RGImageDescription{
                    .Width = (f32)*CVars::Get().GetI32CVar("Atmosphere.AerialPerspective.Size"_hsv),
                    .Height = (f32)*CVars::Get().GetI32CVar("Atmosphere.AerialPerspective.Size"_hsv),
                    .LayersDepth = (f32)*CVars::Get().GetI32CVar("Atmosphere.AerialPerspective.Size"_hsv),
                    .Format = Format::RGBA16_FLOAT,
                    .Kind = ImageKind::Image3d
            }));
            passData.BindGroup.SetResourcesTransmittanceLut(info.TransmittanceLut);
            passData.BindGroup.SetResourcesMultiscatteringLut(info.MultiscatteringLut);
            passData.BindGroup.SetResourcesCsmData(info.CsmData.CsmInfo);
            passData.BindGroup.SetResourcesCsmTexture(info.CsmData.ShadowMap);
            passData.BindGroup.SetResourcesView(info.ViewInfo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Atmosphere.AerialPerspective")
            GPU_PROFILE_FRAME("Atmosphere.AerialPerspective")

            auto& lutDescription = graph.GetImageDescription(passData.Lut);

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
	            .Invocations = {lutDescription.Width, lutDescription.Height, lutDescription.GetDepth()},
	            .GroupSize = passData.BindGroup.GetAerialPerspectiveLutGroupSize()
            });
        });
}
