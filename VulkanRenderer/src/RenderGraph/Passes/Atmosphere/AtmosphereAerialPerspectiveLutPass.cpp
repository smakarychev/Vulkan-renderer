#include "AtmosphereAerialPerspectiveLutPass.h"

#include "cvars/CVarSystem.h"
#include "Light/SceneLight.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/AtmosphereAerialPerspectiveLutBindGroup.generated.h"
#include "RenderGraph/Passes/Generated/ShaderBindGroupBase.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Atmosphere::AerialPerspective::addToGraph(std::string_view name, RG::Graph& renderGraph,
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

            passData.Lut = graph.CreateResource(std::format("{}.Lut", name), GraphTextureDescription{
                .Width = (u32)*CVars::Get().GetI32CVar({"Atmosphere.AerialPerspective.Size"}),
                .Height = (u32)*CVars::Get().GetI32CVar({"Atmosphere.AerialPerspective.Size"}),
                .Layers = (u32)*CVars::Get().GetI32CVar({"Atmosphere.AerialPerspective.Size"}),
                .Format = Format::RGBA16_FLOAT,
                .Kind = ImageKind::Image3d});
            passData.DirectionalLight = graph.AddExternal(std::format("{}.DirectionalLight", name),
                light.GetBuffers().DirectionalLight);

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

            const Texture& lutTexture = resources.GetTexture(passData.Lut);

            const Shader& shader = resources.GetGraph()->GetShader();
            AtmosphereAerialPerspectiveLutShaderBindGroup bindGroup(shader);
            
            bindGroup.SetAtmosphereSettings({.Buffer = resources.GetBuffer(passData.AtmosphereSettings)});
            bindGroup.SetDirectionalLight({.Buffer = resources.GetBuffer(passData.AtmosphereSettings)});
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});
            bindGroup.SetTransmittanceLut(resources.GetTexture(passData.TransmittanceLut).BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            bindGroup.SetMultiscatteringLut(resources.GetTexture(passData.MultiscatteringLut).BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            bindGroup.SetAerialPerspectiveLut(lutTexture.BindingInfo(ImageFilter::Linear, ImageLayout::General));

            RgUtils::updateCSMBindings(bindGroup, resources, passData.CSMData);

            auto& cmd = frameContext.Cmd;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            RenderCommand::Dispatch(cmd,
                {lutTexture.Description().Width, lutTexture.Description().Height, lutTexture.Description().GetDepth()},
                {16, 16, 1});
        });
}
