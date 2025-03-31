#include "AtmosphereMultiscatteringPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/AtmosphereMultiscatteringLutBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

RG::Pass& Passes::Atmosphere::Multiscattering::addToGraph(std::string_view name, RG::Graph& renderGraph,
    RG::Resource transmittanceLut, RG::Resource atmosphereSettings)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Multiscattering.Setup")

            graph.SetShader("atmosphere-multiscattering-lut.shader");

            passData.Lut = graph.CreateResource(std::format("{}.Lut", name), GraphTextureDescription{
                .Width = (u32)*CVars::Get().GetI32CVar("Atmosphere.Multiscattering.Size"_hsv),
                .Height = (u32)*CVars::Get().GetI32CVar("Atmosphere.Multiscattering.Size"_hsv),
                .Format = Format::RGBA16_FLOAT});
            
            passData.AtmosphereSettings = graph.Read(atmosphereSettings, Compute | Uniform);
            passData.TransmittanceLut = graph.Read(transmittanceLut, Compute | Sampled);
            passData.Lut = graph.Write(passData.Lut, Compute | Storage);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Atmosphere.Multiscattering")
            GPU_PROFILE_FRAME("Atmosphere.Multiscattering")

            auto&& [lutTexture, lutDescription] = resources.GetTextureWithDescription(passData.Lut);

            const Shader& shader = resources.GetGraph()->GetShader();
            AtmosphereMultiscatteringLutShaderBindGroup bindGroup(shader);
            bindGroup.SetAtmosphereSettings({.Buffer = resources.GetBuffer(passData.AtmosphereSettings)});
            bindGroup.SetTransmittanceLut({.Image = resources.GetTexture(passData.TransmittanceLut)},
                ImageLayout::Readonly);
            bindGroup.SetMultiscatteringLut({.Image = lutTexture}, ImageLayout::General);

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetArenaAllocators());
            frameContext.CommandList.Dispatch({
				.Invocations = {lutDescription.Width, lutDescription.Height, 64},
				.GroupSize = {1, 1, 64}});
        });
}
