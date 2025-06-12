#include "AtmosphereTransmittanceLutPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/AtmosphereTransmittanceLutBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

Passes::Atmosphere::Transmittance::PassData& Passes::Atmosphere::Transmittance::addToGraph(StringId name,
    RG::Graph& renderGraph, RG::Resource viewInfo)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Transmittance.Setup")

            graph.SetShader("atmosphere-transmittance-lut"_hsv);

            passData.Lut = graph.Create("Lut"_hsv, RGImageDescription{
                .Width = (f32)*CVars::Get().GetI32CVar("Atmosphere.Transmittance.Width"_hsv),
                .Height = (f32)*CVars::Get().GetI32CVar("Atmosphere.Transmittance.Height"_hsv),
                .Format = Format::RGBA16_FLOAT});
                
            passData.ViewInfo = graph.ReadBuffer(viewInfo, Compute | Uniform);
            passData.Lut = graph.WriteImage(passData.Lut, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Atmosphere.Transmittance")
            GPU_PROFILE_FRAME("Atmosphere.Transmittance")

            auto& lutDescription = graph.GetImageDescription(passData.Lut);

            const Shader& shader = graph.GetShader();
            AtmosphereTransmittanceLutShaderBindGroup bindGroup(shader);
            bindGroup.SetViewInfo(graph.GetBufferBinding(passData.ViewInfo));
            bindGroup.SetLut(graph.GetImageBinding(passData.Lut));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.Dispatch({
				.Invocations = {lutDescription.Width, lutDescription.Height, 1},
				.GroupSize = {16, 16, 1}});
        });
}
