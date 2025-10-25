#include "rendererpch.h"

#include "AtmosphereAerialPerspectiveLutPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/AtmosphereAerialPerspectiveLutBindGroup.generated.h"
#include "RenderGraph/Passes/Generated/ShaderBindGroupBase.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/SceneLight.h"

Passes::Atmosphere::AerialPerspective::PassData& Passes::Atmosphere::AerialPerspective::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.AerialPerspective.Setup")

            graph.SetShader("atmosphere-aerial-perspective-lut"_hsv);

            passData.AerialPerspective = graph.Create("Lut"_hsv, RGImageDescription{
                .Width = (f32)*CVars::Get().GetI32CVar("Atmosphere.AerialPerspective.Size"_hsv),
                .Height = (f32)*CVars::Get().GetI32CVar("Atmosphere.AerialPerspective.Size"_hsv),
                .LayersDepth = (f32)*CVars::Get().GetI32CVar("Atmosphere.AerialPerspective.Size"_hsv),
                .Format = Format::RGBA16_FLOAT,
                .Kind = ImageKind::Image3d});
            passData.DirectionalLight = graph.Import("DirectionalLight"_hsv,
                info.Light->GetBuffers().DirectionalLights);

            passData.TransmittanceLut = graph.ReadImage(info.TransmittanceLut, Compute | Sampled);
            passData.MultiscatteringLut = graph.ReadImage(info.MultiscatteringLut, Compute | Sampled);
            passData.DirectionalLight = graph.ReadBuffer(passData.DirectionalLight, Compute | Uniform);
            passData.ViewInfo = graph.ReadBuffer(info.ViewInfo, Compute | Uniform);
            passData.CsmData = RgUtils::readCsmData(info.CsmData, graph, Compute);
            passData.AerialPerspective = graph.WriteImage(passData.AerialPerspective, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Atmosphere.AerialPerspective")
            GPU_PROFILE_FRAME("Atmosphere.AerialPerspective")

            auto&& [lutTexture, lutDescription] = graph.GetImageWithDescription(passData.AerialPerspective);

            const Shader& shader = graph.GetShader();
            AtmosphereAerialPerspectiveLutShaderBindGroup bindGroup(shader);
            
            bindGroup.SetViewInfo(graph.GetBufferBinding(passData.ViewInfo));
            bindGroup.SetDirectionalLights(graph.GetBufferBinding(passData.DirectionalLight));
            bindGroup.SetTransmittanceLut(graph.GetImageBinding(passData.TransmittanceLut));
            bindGroup.SetMultiscatteringLut(graph.GetImageBinding(passData.MultiscatteringLut));
            bindGroup.SetAerialPerspectiveLut(graph.GetImageBinding(passData.AerialPerspective));

            RgUtils::updateCsmBindings(bindGroup, graph, passData.CsmData);

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.Dispatch({
	            .Invocations = {lutDescription.Width, lutDescription.Height, lutDescription.GetDepth()},
	            .GroupSize = {16, 16, 1}});
        });
}
