#include "CloudReprojectPass.h"

#include "CloudsCommon.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/CloudReprojectBindGroup.generated.h"
#include "VerticalProfile/VPCloudsPass.h"

Passes::CloudReproject::PassData& Passes::CloudReproject::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Clouds.Reproject.Setup")

            graph.SetShader("cloud-reproject"_hsv);

            if (!info.ColorAccumulationIn.IsValid())
            {
                passData.ColorAccumulationIn = graph.Create("Color.Accumulation.In"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Size2d | RGImageInference::Format,
                    .Width = Clouds::REPROJECTION_RELATIVE_SIZE_INV,
                    .Height = Clouds::REPROJECTION_RELATIVE_SIZE_INV,
                    .Reference = info.Color,
                });
                passData.DepthAccumulationIn = graph.Create("Depth.Accumulation.In"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Size2d | RGImageInference::Format,
                    .Width = Clouds::REPROJECTION_RELATIVE_SIZE_INV,
                    .Height = Clouds::REPROJECTION_RELATIVE_SIZE_INV,
                    .Reference = info.Depth,
                });
                passData.ReprojectionFactorIn = graph.Create("Depth.ReprojectionFactor.In"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Size2d,
                    .Width = Clouds::REPROJECTION_RELATIVE_SIZE_INV,
                    .Height = Clouds::REPROJECTION_RELATIVE_SIZE_INV,
                    .Reference = info.Color,
                    .Format = Format::R8_UNORM,
                });
                passData.ColorAccumulationOut = graph.Create("Color.Accumulation"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Full,
                    .Reference = passData.ColorAccumulationIn,
                });
                passData.DepthAccumulationOut = graph.Create("Depth.Accumulation"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Full,
                    .Reference = passData.DepthAccumulationIn,
                });
                passData.ReprojectionFactorOut = graph.Create("Depth.ReprojectionFactor"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Full,
                    .Reference = passData.ReprojectionFactorIn,
                });
            }
            else
            {
                passData.ColorAccumulationIn = info.ColorAccumulationIn;
                passData.DepthAccumulationIn = info.DepthAccumulationIn;
                passData.ReprojectionFactorIn = info.ReprojectionFactorIn;
                passData.ColorAccumulationOut = info.ColorAccumulationOut;
                passData.DepthAccumulationOut = info.DepthAccumulationOut;
                passData.ReprojectionFactorOut = info.ReprojectionFactorOut;
            }

            passData.ViewInfo = graph.ReadBuffer(info.ViewInfo, Compute | Uniform);
            passData.Color = graph.ReadImage(info.Color, Compute | Sampled);
            passData.Depth = graph.ReadImage(info.Depth, Compute | Sampled);
            passData.ColorAccumulationIn = graph.ReadWriteImage(passData.ColorAccumulationIn, Compute | Sampled | Storage);
            passData.DepthAccumulationIn = graph.ReadWriteImage(passData.DepthAccumulationIn, Compute | Sampled | Storage);
            passData.ReprojectionFactorIn = graph.ReadWriteImage(passData.ReprojectionFactorIn, Compute | Sampled | Storage);
            passData.ColorAccumulationOut = graph.WriteImage(passData.ColorAccumulationOut, Compute | Storage);
            passData.DepthAccumulationOut = graph.WriteImage(passData.DepthAccumulationOut, Compute | Storage);
            passData.ReprojectionFactorOut = graph.WriteImage(passData.ReprojectionFactorOut, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Clouds.Reproject")
            GPU_PROFILE_FRAME("Clouds.Reproject")

            const glm::uvec2 resolution = graph.GetImageDescription(passData.ColorAccumulationOut).Dimensions();
            
            const Shader& shader = graph.GetShader();
            CloudReprojectShaderBindGroup bindGroup(shader);
            bindGroup.SetViewInfo(graph.GetBufferBinding(passData.ViewInfo));
            bindGroup.SetCloudColor(graph.GetImageBinding(passData.Color));
            bindGroup.SetCloudDepth(graph.GetImageBinding(passData.Depth));
            bindGroup.SetCloudColorAccumulationIn(graph.GetImageBinding(passData.ColorAccumulationIn));
            bindGroup.SetCloudDepthAccumulationIn(graph.GetImageBinding(passData.DepthAccumulationIn));
            bindGroup.SetCloudReprojectionFactorIn(graph.GetImageBinding(passData.ReprojectionFactorIn));
            bindGroup.SetCloudColorAccumulationOut(graph.GetImageBinding(passData.ColorAccumulationOut));
            bindGroup.SetCloudDepthAccumulationOut(graph.GetImageBinding(passData.DepthAccumulationOut));
            bindGroup.SetCloudReprojectionFactorOut(graph.GetImageBinding(passData.ReprojectionFactorOut));

            struct PushConstant
            {
                f32 WindAngle{};
                f32 WindSpeed{};
                f32 WindUpright{};
                f32 WindSkew{};
            };

            PushConstant pushConstant = {
                .WindAngle = info.CloudParameters->WindAngle,
                .WindSpeed = info.CloudParameters->WindSpeed,
                .WindUpright = info.CloudParameters->WindUprightAmount,
                .WindSkew = info.CloudParameters->WindHorizontalSkew};
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(),
                .Data = {pushConstant}
            });
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = {8, 8, 1}
            });
        });
}
