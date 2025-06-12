#include "AtmosphereRaymarchPass.h"

#include "ViewInfoGPU.h"
#include "Core/Camera.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/AtmosphereRaymarchBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/SceneLight.h"

Passes::Atmosphere::Raymarch::PassData& Passes::Atmosphere::Raymarch::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Raymarch.Setup")

            graph.SetShader("atmosphere-raymarch"_hsv);

            passData.DirectionalLight = graph.Import("DirectionalLight"_hsv,
                info.Light->GetBuffers().DirectionalLights);
            passData.ColorOut = info.ColorIn;

            if (info.DepthIn.IsValid())
                passData.DepthIn = graph.ReadImage(info.DepthIn, Pixel | Sampled);

            passData.SkyViewLut = graph.ReadImage(info.SkyViewLut, Pixel | Sampled);
            if (info.TransmittanceLut.IsValid())
                passData.TransmittanceLut = graph.ReadImage(info.TransmittanceLut, Pixel | Sampled);
            if (info.AerialPerspective.IsValid())
                passData.AerialPerspective = graph.ReadImage(info.AerialPerspective, Pixel | Sampled);
            passData.ViewInfo = graph.ReadBuffer(info.ViewInfo, Pixel | Uniform);
            passData.DirectionalLight = graph.ReadBuffer(passData.DirectionalLight, Pixel | Uniform);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, {});
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Atmosphere.Raymarch")
            GPU_PROFILE_FRAME("Atmosphere.Raymarch")

            const Shader& shader = graph.GetShader();
            AtmosphereRaymarchShaderBindGroup bindGroup(shader);
            if (passData.DepthIn.IsValid())
                bindGroup.SetDepth(graph.GetImageBinding(passData.DepthIn));

            bindGroup.SetViewInfo(graph.GetBufferBinding(passData.ViewInfo));
            bindGroup.SetDirectionalLights(graph.GetBufferBinding(passData.DirectionalLight));
            bindGroup.SetSkyViewLut(graph.GetImageBinding(passData.SkyViewLut));
            if (passData.TransmittanceLut.IsValid())
                bindGroup.SetTransmittanceLut(graph.GetImageBinding(passData.TransmittanceLut));
            if (passData.AerialPerspective.IsValid())
                bindGroup.SetAerialPerspectiveLut(graph.GetImageBinding(passData.AerialPerspective));

            struct PushConstant
            {
                GpuBool UseDepthBuffer;
                GpuBool UseSunLuminance;
            };
            PushConstant pushConstant = {
                .UseDepthBuffer = passData.DepthIn.IsValid(),
                .UseSunLuminance = info.UseSunLuminance};
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {pushConstant}});
            cmd.Draw({.VertexCount = 3});
        });
}
