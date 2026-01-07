#include "rendererpch.h"

#include "VPCloudPass.h"

#include "RenderGraph/Passes/Clouds/CloudCommon.h"
#include "RenderGraph/Passes/Generated/CloudsVPRenderBindGroupRG.generated.h"

Passes::Clouds::VP::PassData& Passes::Clouds::VP::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, CloudsVPRenderBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("VP.Cloud.Setup")

            passData.BindGroup = CloudsVPRenderBindGroupRG(graph,
                info.IsEnvironmentCapture ?
                    CloudsVPRenderBindGroupRG::Variants::EnvironmentView :
                    CloudsVPRenderBindGroupRG::Variants::PrimaryView,
                ShaderOverrides(
                    ShaderDefines({
                        ShaderDefine("REPROJECTION"_hsv, info.CloudsRenderingMode == CloudsRenderingMode::Reprojection)
            })));
            
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

            passData.ColorOut = passData.BindGroup.SetResourcesColor(passData.ColorOut);
            if (!info.IsEnvironmentCapture)
                passData.DepthOut = passData.BindGroup.SetResourcesOutDepth(passData.DepthOut);

            passData.BindGroup.SetResourcesCoverage(info.CloudCoverage);
            passData.BindGroup.SetResourcesProfile(info.CloudProfile);
            passData.BindGroup.SetResourcesLowFrequency(info.CloudShapeLowFrequencyMap);
            passData.BindGroup.SetResourcesHighFrequency(info.CloudShapeHighFrequencyMap);
            passData.BindGroup.SetResourcesCurlNoise(info.CloudCurlNoise);
            passData.BindGroup.SetResourcesParameters(info.CloudParameters);
            passData.BindGroup.SetResourcesView(info.ViewInfo);
            passData.BindGroup.SetResourcesIrradiance(info.IrradianceSH);
            
            if (!info.IsEnvironmentCapture)
            {
                passData.BindGroup.SetResourcesDepth(info.DepthIn);
                passData.BindGroup.SetResourcesDepthMinMax(info.MinMaxDepthIn);
                passData.BindGroup.SetResourcesAerialPerspectiveLut(info.AerialPerspectiveLut);
            }
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("VP.Cloud")
            GPU_PROFILE_FRAME("VP.Cloud")

            const glm::uvec2 resolution = graph.GetImageDescription(passData.ColorOut).Dimensions();

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = passData.BindGroup.GetCloudsVPRenderGroupSize()
            });
        });
}
