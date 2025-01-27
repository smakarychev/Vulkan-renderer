#include "AtmosphereTransmittanceLutPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/AtmosphereTransmittanceLutBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

RG::Pass& Passes::Atmosphere::Transmittance::addToGraph(std::string_view name, RG::Graph& renderGraph,
    RG::Resource atmosphereSettings)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Transmittance.Setup")

            graph.SetShader("atmosphere-transmittance-lut.shader");

            passData.Lut = graph.CreateResource(std::format("{}.Lut", name), GraphTextureDescription{
                .Width = (u32)*CVars::Get().GetI32CVar({"Atmosphere.Transmittance.Width"}),
                .Height = (u32)*CVars::Get().GetI32CVar({"Atmosphere.Transmittance.Height"}),
                .Format = Format::RGBA16_FLOAT});
                
            passData.AtmosphereSettings = graph.Read(atmosphereSettings, Compute | Uniform);
            passData.Lut = graph.Write(passData.Lut, Compute | Storage);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Atmosphere.Transmittance")
            GPU_PROFILE_FRAME("Atmosphere.Transmittance")

            const Texture& lutTexture = resources.GetTexture(passData.Lut);

            const Shader& shader = resources.GetGraph()->GetShader();
            AtmosphereTransmittanceLutShaderBindGroup bindGroup(shader);
            bindGroup.SetAtmosphereSettings(resources.GetBuffer(passData.AtmosphereSettings).BindingInfo());
            bindGroup.SetLut(lutTexture.BindingInfo(ImageFilter::Linear, ImageLayout::General));

            auto& cmd = frameContext.Cmd;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            RenderCommand::Dispatch(cmd,
                {lutTexture.Description().Width, lutTexture.Description().Height, 1},
                {16, 16, 1});
        });
}
