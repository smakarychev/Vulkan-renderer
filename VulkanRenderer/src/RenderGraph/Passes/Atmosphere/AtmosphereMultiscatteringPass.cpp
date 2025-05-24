#include "AtmosphereMultiscatteringPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/AtmosphereMultiscatteringLutBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

Passes::Atmosphere::Multiscattering::PassData& Passes::Atmosphere::Multiscattering::addToGraph(StringId name,
    RG::Graph& renderGraph, RG::Resource transmittanceLut, RG::Resource atmosphereSettings)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Multiscattering.Setup")

            graph.SetShader("atmosphere-multiscattering-lut"_hsv);

            passData.Lut = graph.Create("Lut"_hsv, RGImageDescription{
                .Width = (f32)*CVars::Get().GetI32CVar("Atmosphere.Multiscattering.Size"_hsv),
                .Height = (f32)*CVars::Get().GetI32CVar("Atmosphere.Multiscattering.Size"_hsv),
                .Format = Format::RGBA16_FLOAT});
            
            passData.AtmosphereSettings = graph.ReadBuffer(atmosphereSettings, Compute | Uniform);
            passData.TransmittanceLut = graph.ReadImage(transmittanceLut, Compute | Sampled);
            passData.Lut = graph.WriteImage(passData.Lut, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Atmosphere.Multiscattering")
            GPU_PROFILE_FRAME("Atmosphere.Multiscattering")

            auto&& [lutTexture, lutDescription] = graph.GetImageWithDescription(passData.Lut);

            const Shader& shader = graph.GetShader();
            AtmosphereMultiscatteringLutShaderBindGroup bindGroup(shader);
            bindGroup.SetAtmosphereSettings(graph.GetBufferBinding(passData.AtmosphereSettings));
            bindGroup.SetTransmittanceLut(graph.GetImageBinding(passData.TransmittanceLut));
            bindGroup.SetMultiscatteringLut(graph.GetImageBinding(passData.Lut));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.Dispatch({
				.Invocations = {lutDescription.Width, lutDescription.Height, 64},
				.GroupSize = {1, 1, 64}});
        });
}
