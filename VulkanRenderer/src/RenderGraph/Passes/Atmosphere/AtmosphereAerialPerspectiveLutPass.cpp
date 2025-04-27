#include "AtmosphereAerialPerspectiveLutPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/AtmosphereAerialPerspectiveLutBindGroup.generated.h"
#include "RenderGraph/Passes/Generated/ShaderBindGroupBase.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/SceneLight.h"

RG::Pass& Passes::Atmosphere::AerialPerspective::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource transmittanceLut, RG::Resource multiscatteringLut,
    RG::Resource atmosphereSettings, const SceneLight& light, const RG::CSMData& csmData)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.AerialPerspective.Setup")

            graph.SetShader("atmosphere-aerial-perspective-lut.shader");

            auto& globalResources = graph.GetGlobalResources();

            passData.Lut = graph.CreateResource("Lut"_hsv, GraphTextureDescription{
                .Width = (u32)*CVars::Get().GetI32CVar("Atmosphere.AerialPerspective.Size"_hsv),
                .Height = (u32)*CVars::Get().GetI32CVar("Atmosphere.AerialPerspective.Size"_hsv),
                .Layers = (u32)*CVars::Get().GetI32CVar("Atmosphere.AerialPerspective.Size"_hsv),
                .Format = Format::RGBA16_FLOAT,
                .Kind = ImageKind::Image3d});
            passData.DirectionalLight = graph.AddExternal("DirectionalLight"_hsv,
                light.GetBuffers().DirectionalLights);

            passData.AtmosphereSettings = graph.Read(atmosphereSettings, Compute | Uniform);
            passData.TransmittanceLut = graph.Read(transmittanceLut, Compute | Sampled);
            passData.MultiscatteringLut = graph.Read(multiscatteringLut, Compute | Sampled);
            passData.DirectionalLight = graph.Read(passData.DirectionalLight, Compute | Uniform);
            passData.Camera = graph.Read(globalResources.PrimaryCameraGPU, Compute | Uniform);
            passData.CSMData = RgUtils::readCSMData(csmData, graph, Compute);
            passData.Lut = graph.Write(passData.Lut, Compute | Storage);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Atmosphere.AerialPerspective")
            GPU_PROFILE_FRAME("Atmosphere.AerialPerspective")

            auto&& [lutTexture, lutDescription] = resources.GetTextureWithDescription(passData.Lut);

            const Shader& shader = resources.GetGraph()->GetShader();
            AtmosphereAerialPerspectiveLutShaderBindGroup bindGroup(shader);
            
            bindGroup.SetAtmosphereSettings({.Buffer = resources.GetBuffer(passData.AtmosphereSettings)});
            bindGroup.SetDirectionalLight({.Buffer = resources.GetBuffer(passData.AtmosphereSettings)});
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});
            bindGroup.SetTransmittanceLut({.Image = resources.GetTexture(passData.TransmittanceLut)},
                ImageLayout::Readonly);
            bindGroup.SetMultiscatteringLut({.Image = resources.GetTexture(passData.MultiscatteringLut)},
                ImageLayout::Readonly);
            bindGroup.SetAerialPerspectiveLut({.Image = lutTexture}, ImageLayout::General);

            RgUtils::updateCSMBindings(bindGroup, resources, passData.CSMData);

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetArenaAllocators());
            cmd.Dispatch({
	            .Invocations = {lutDescription.Width, lutDescription.Height, lutDescription.GetDepth()},
	            .GroupSize = {16, 16, 1}});
        });
}
