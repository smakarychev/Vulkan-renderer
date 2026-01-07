#include "rendererpch.h"

#include "CloudReprojectPass.h"

#include "CloudCommon.h"
#include "RenderGraph/Passes/Generated/CloudsReprojectBindGroupRG.generated.h"

Passes::Clouds::Reproject::PassData& Passes::Clouds::Reproject::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, CloudsReprojectBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Cloud.Reproject.Setup")

            passData.BindGroup = CloudsReprojectBindGroupRG(graph);

            Resource colorIn, colorOut, depthIn, depthOut, reprojectionIn, reprojectionOut;

            if (!info.ColorAccumulationIn.IsValid())
            {
                colorIn = graph.Create("Color.Accumulation.In"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Size2d | RGImageInference::Format,
                    .Width = REPROJECTION_RELATIVE_SIZE_INV,
                    .Height = REPROJECTION_RELATIVE_SIZE_INV,
                    .Reference = info.Color,
                });
                depthIn = graph.Create("Depth.Accumulation.In"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Size2d | RGImageInference::Format,
                    .Width = REPROJECTION_RELATIVE_SIZE_INV,
                    .Height = REPROJECTION_RELATIVE_SIZE_INV,
                    .Reference = info.Depth,
                });
                reprojectionIn = graph.Create("ReprojectionFactor.In"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Size2d,
                    .Width = REPROJECTION_RELATIVE_SIZE_INV,
                    .Height = REPROJECTION_RELATIVE_SIZE_INV,
                    .Reference = info.Color,
                    .Format = Format::R8_UNORM,
                });

                colorOut = graph.Create("Color.Accumulation"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Full,
                    .Reference = colorIn
                });
                depthOut = graph.Create("Depth.Accumulation"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Full,
                    .Reference = depthIn,
                });
                reprojectionOut = graph.Create("ReprojectionFactor"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Full,
                    .Reference = reprojectionIn,
                });
            }
            else
            {
                colorIn = info.ColorAccumulationIn;
                depthIn = info.DepthAccumulationIn;
                reprojectionIn = info.ReprojectionFactorIn;
                colorOut = info.ColorAccumulationOut;
                depthOut = info.DepthAccumulationOut;
                reprojectionOut = info.ReprojectionFactorOut;
            }

            passData.ColorAccumulation = passData.BindGroup.SetResourcesCloudColorAccumulationOut(colorOut);
            passData.DepthAccumulation = passData.BindGroup.SetResourcesCloudDepthAccumulationOut(depthOut);
            passData.ReprojectionFactor = passData.BindGroup.SetResourcesCloudReprojectionFactorOut(reprojectionOut);
            passData.ColorAccumulationPrevious = passData.BindGroup.SetResourcesCloudColorAccumulationIn(colorIn);
            passData.DepthAccumulationPrevious = passData.BindGroup.SetResourcesCloudDepthAccumulationIn(depthIn);
            passData.ReprojectionFactorPrevious =
                passData.BindGroup.SetResourcesCloudReprojectionFactorIn(reprojectionIn);
            
            passData.BindGroup.SetResourcesCloudColor(info.Color);
            passData.BindGroup.SetResourcesCloudDepth(info.Depth);
            passData.BindGroup.SetResourcesSceneDepth(info.SceneDepth);
            passData.BindGroup.SetResourcesClouds(info.CloudParameters);
            passData.BindGroup.SetResourcesView(info.ViewInfo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Cloud.Reproject")
            GPU_PROFILE_FRAME("Cloud.Reproject")

            const glm::uvec2 resolution = graph.GetImageDescription(passData.ColorAccumulation).Dimensions();
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = passData.BindGroup.GetCloudReprojectionGroupSize()
            });
        });
}
