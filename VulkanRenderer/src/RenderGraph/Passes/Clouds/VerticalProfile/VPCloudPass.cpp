#include "VPCloudPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Clouds/CloudCommon.h"
#include "RenderGraph/Passes/Generated/CloudVpBindGroup.generated.h"
#include "Scene/SceneLight.h"

Passes::Clouds::VP::PassData& Passes::Clouds::VP::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("VP.Cloud.Setup")

            graph.SetShader("cloud-vp"_hsv, ShaderOverrides{
                ShaderDefines({
                    ShaderDefine("REPROJECTION"_hsv, info.CloudsRenderingMode == CloudsRenderingMode::Reprojection),
                    ShaderDefine("ENVIRONMENT_CAPTURE"_hsv, info.IsEnvironmentCapture),
                })
            });

            passData.DirectionalLights = graph.Import("DirectionalLight"_hsv,
                info.Light->GetBuffers().DirectionalLights);

            const f32 relativeSize = info.CloudsRenderingMode == CloudsRenderingMode::Reprojection ?
                REPROJECTION_RELATIVE_SIZE : 1.0f;
            if (!info.ColorOut.IsValid())
                passData.ColorOut = graph.Create("Clouds.Color"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Size2d,
                    .Width = relativeSize,
                    .Height = relativeSize,
                    .Reference = graph.GetBackbufferImage(),
                    .Format = Format::RGBA16_FLOAT,
                });
            else
                passData.ColorOut = info.ColorOut;
            if (!info.DepthOut.IsValid() && !info.IsEnvironmentCapture)
                passData.DepthOut = graph.Create("Clouds.Depth"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Size2d,
                    .Width = relativeSize,
                    .Height = relativeSize,
                    .Reference = graph.GetBackbufferImage(),
                    .Format = Format::RG16_FLOAT,
                });
            else
                passData.DepthOut = info.DepthOut;
      
            passData.CloudCoverage = graph.ReadImage(info.CloudCoverage, Compute | Sampled);
            passData.CloudProfile = graph.ReadImage(info.CloudProfile, Compute | Sampled);
            passData.CloudShapeLowFrequencyMap = graph.ReadImage(info.CloudShapeLowFrequencyMap, Compute | Sampled);
            passData.CloudShapeHighFrequencyMap = graph.ReadImage(info.CloudShapeHighFrequencyMap, Compute | Sampled);
            passData.CloudCurlNoise = graph.ReadImage(info.CloudCurlNoise, Compute | Sampled);
            if (!info.IsEnvironmentCapture)
            {
                passData.AerialPerspectiveLut = graph.ReadImage(info.AerialPerspectiveLut, Compute | Sampled);
                passData.DepthIn = graph.ReadImage(info.DepthIn, Compute | Sampled);
                passData.MinMaxDepthIn = graph.ReadImage(info.MinMaxDepthIn, Compute | Sampled);
                passData.DepthOut = graph.WriteImage(passData.DepthOut, Compute | Storage);
            }
            passData.ColorOut = graph.WriteImage(passData.ColorOut, Compute | Storage);
            passData.ViewInfo = graph.ReadBuffer(info.ViewInfo, Compute | Uniform);
            passData.IrradianceSH = graph.ReadBuffer(info.IrradianceSH, Compute | Uniform);
            passData.DirectionalLights = graph.ReadBuffer(passData.DirectionalLights, Compute | Uniform);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("VP.Cloud")
            GPU_PROFILE_FRAME("VP.Cloud")

            const glm::uvec2 resolution = graph.GetImageDescription(passData.ColorOut).Dimensions();

            const Shader& shader = graph.GetShader();
            CloudVpShaderBindGroup bindGroup(shader);
            bindGroup.SetViewInfo(graph.GetBufferBinding(passData.ViewInfo));
            bindGroup.SetCloudCoverage(graph.GetImageBinding(passData.CloudCoverage));
            bindGroup.SetCloudProfile(graph.GetImageBinding(passData.CloudProfile));
            bindGroup.SetCloudLowFrequency(graph.GetImageBinding(passData.CloudShapeLowFrequencyMap));
            bindGroup.SetCloudHighFrequency(graph.GetImageBinding(passData.CloudShapeHighFrequencyMap));
            bindGroup.SetCloudCurlNoise(graph.GetImageBinding(passData.CloudCurlNoise));
            if (!info.IsEnvironmentCapture)
            {
                bindGroup.SetDepth(graph.GetImageBinding(passData.DepthIn));
                bindGroup.SetMinMaxDepth(graph.GetImageBinding(passData.MinMaxDepthIn));
                bindGroup.SetAerialPerspectiveLut(graph.GetImageBinding(passData.AerialPerspectiveLut));
                bindGroup.SetOutDepth(graph.GetImageBinding(passData.DepthOut));
            }
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
