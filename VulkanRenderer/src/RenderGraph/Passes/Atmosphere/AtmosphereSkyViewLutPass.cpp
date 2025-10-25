#include "rendererpch.h"

#include "AtmosphereSkyViewLutPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/Generated/AtmosphereSkyViewLutBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/SceneLight.h"

Passes::Atmosphere::SkyView::PassData& Passes::Atmosphere::SkyView::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.SkyView.Setup")

            graph.SetShader("atmosphere-sky-view-lut"_hsv);

            passData.Lut = graph.Create("Lut"_hsv, RGImageDescription{
                .Width = (f32)*CVars::Get().GetI32CVar("Atmosphere.SkyView.Width"_hsv),
                .Height = (f32)*CVars::Get().GetI32CVar("Atmosphere.SkyView.Height"_hsv),
                .Format = Format::RGBA16_FLOAT});
            passData.DirectionalLight = graph.Import("DirectionalLight"_hsv,
                info.Light->GetBuffers().DirectionalLights);
            
            passData.ViewInfo = graph.ReadBuffer(info.ViewInfo, Compute | Uniform);
            passData.TransmittanceLut = graph.ReadImage(info.TransmittanceLut, Compute | Sampled);
            passData.MultiscatteringLut = graph.ReadImage(info.MultiscatteringLut, Compute | Sampled);
            passData.DirectionalLight = graph.ReadBuffer(passData.DirectionalLight, Compute | Uniform);
            passData.Lut = graph.WriteImage(passData.Lut, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Atmosphere.SkyView")
            GPU_PROFILE_FRAME("Atmosphere.SkyView")

            auto& lutDescription = graph.GetImageDescription(passData.Lut);
            
            const Shader& shader = graph.GetShader();
            AtmosphereSkyViewLutShaderBindGroup bindGroup(shader);
            bindGroup.SetViewInfo(graph.GetBufferBinding(passData.ViewInfo));
            bindGroup.SetDirectionalLights(graph.GetBufferBinding(passData.DirectionalLight));
            bindGroup.SetTransmittanceLut(graph.GetImageBinding(passData.TransmittanceLut));
            bindGroup.SetMultiscatteringLut(graph.GetImageBinding(passData.MultiscatteringLut));
            bindGroup.SetSkyViewLut(graph.GetImageBinding(passData.Lut));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.Dispatch({
				.Invocations = {lutDescription.Width, lutDescription.Height, 1},
				.GroupSize = {16, 16, 1}});
        });
}
