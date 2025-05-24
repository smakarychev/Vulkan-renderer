#include "AtmosphereRaymarchPass.h"

#include "CameraGPU.h"
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
            passData.ColorOut = RgUtils::ensureResource(info.ColorIn, graph, "ColorOut"_hsv,
                RGImageDescription{
                    .Width = (f32)info.Camera->GetViewportWidth(),
                    .Height = (f32)info.Camera->GetViewportHeight(),
                    .Format = Format::RGBA16_FLOAT});

            passData.Camera = graph.Create("Camera"_hsv, RGBufferDescription{
                .SizeBytes = sizeof(CameraGPU)});
            graph.Upload(passData.Camera, CameraGPU::FromCamera(*info.Camera,
                glm::vec2(info.Camera->GetViewportWidth(), info.Camera->GetViewportHeight())));

            if (info.DepthIn.IsValid())
                passData.DepthIn = graph.ReadImage(info.DepthIn, Pixel | Sampled);

            passData.SkyViewLut = graph.ReadImage(info.SkyViewLut, Pixel | Sampled);
            if (info.TransmittanceLut.IsValid())
                passData.TransmittanceLut = graph.ReadImage(info.TransmittanceLut, Pixel | Sampled);
            if (info.AerialPerspective.IsValid())
                passData.AerialPerspective = graph.ReadImage(info.AerialPerspective, Pixel | Sampled);
            passData.AtmosphereSettings = graph.ReadBuffer(info.AtmosphereSettings, Pixel | Uniform);
            passData.DirectionalLight = graph.ReadBuffer(passData.DirectionalLight, Pixel | Uniform);
            passData.Camera = graph.ReadBuffer(passData.Camera, Pixel | Uniform);
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

            bindGroup.SetAtmosphereSettings(graph.GetBufferBinding(passData.AtmosphereSettings));
            bindGroup.SetDirectionalLights(graph.GetBufferBinding(passData.DirectionalLight));
            bindGroup.SetCamera(graph.GetBufferBinding(passData.Camera));
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
