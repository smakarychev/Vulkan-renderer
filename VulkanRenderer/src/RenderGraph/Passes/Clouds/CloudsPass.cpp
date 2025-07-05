#include "CloudsPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/CloudsBindGroup.generated.h"
#include "Scene/SceneLight.h"

Passes::Clouds::PassData& Passes::Clouds::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Clouds.Setup")

            graph.SetShader("clouds"_hsv);

            passData.DirectionalLights = graph.Import("DirectionalLight"_hsv,
                info.Light->GetBuffers().DirectionalLights);

            passData.CloudMap = graph.ReadImage(info.CloudMap, Compute | Sampled);
            passData.CloudShapeLowFrequencyMap = graph.ReadImage(info.CloudShapeLowFrequencyMap, Compute | Sampled);
            passData.CloudShapeHighFrequencyMap = graph.ReadImage(info.CloudShapeHighFrequencyMap, Compute | Sampled);
            passData.CloudCurlNoise = graph.ReadImage(info.CloudCurlNoise, Compute | Sampled);
            passData.AerialPerspectiveLut = graph.ReadImage(info.AerialPerspectiveLut, Compute | Sampled);
            passData.DepthIn = graph.ReadImage(info.DepthIn, Compute | Sampled);
            passData.ViewInfo = graph.ReadBuffer(info.ViewInfo, Compute | Uniform);
            passData.ColorOut = graph.ReadWriteImage(info.ColorIn, Compute | Storage);
            passData.IrradianceSH = graph.ReadBuffer(info.IrradianceSH, Compute | Uniform);
            passData.DirectionalLights = graph.ReadBuffer(passData.DirectionalLights, Compute | Uniform);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Clouds")
            GPU_PROFILE_FRAME("Clouds")

            const glm::uvec2 resolution = graph.GetImageDescription(passData.ColorOut).Dimensions();

            const Shader& shader = graph.GetShader();
            CloudsShaderBindGroup bindGroup(shader);
            bindGroup.SetViewInfo(graph.GetBufferBinding(passData.ViewInfo));
            bindGroup.SetCloudMap(graph.GetImageBinding(passData.CloudMap));
            bindGroup.SetCloudLowFrequency(graph.GetImageBinding(passData.CloudShapeLowFrequencyMap));
            bindGroup.SetCloudHighFrequency(graph.GetImageBinding(passData.CloudShapeHighFrequencyMap));
            bindGroup.SetCloudCurlNoise(graph.GetImageBinding(passData.CloudCurlNoise));
            bindGroup.SetDepth(graph.GetImageBinding(passData.DepthIn));
            bindGroup.SetAerialPerspectiveLut(graph.GetImageBinding(passData.AerialPerspectiveLut));
            bindGroup.SetOutColor(graph.GetImageBinding(passData.ColorOut));
            bindGroup.SetIrradianceSH(graph.GetBufferBinding(passData.IrradianceSH));
            bindGroup.SetDirectionalLights(graph.GetBufferBinding(passData.DirectionalLights));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());

            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(),
                .Data = {*info.CloudParameters}
            });
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = {8, 8, 1}
            });
        });
}
