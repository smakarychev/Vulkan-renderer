#include "VPCloudEnvironmentPass.h"

#include "ViewInfoGPU.h"
#include "Core/Camera.h"
#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Clouds/CloudCommon.h"
#include "RenderGraph/Passes/Generated/CloudVpEnvironmentBlurComposeBindGroup.generated.h"

namespace
{
    Passes::Clouds::VP::Environment::PassData& renderPass(StringId name, RG::Graph& renderGraph,
        const Passes::Clouds::VP::Environment::ExecutionInfo& info)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        using PassData = Passes::Clouds::VP::Environment::PassData;
        return renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("VP.Environment.Setup")

                f32 environmentSize = (f32)*CVars::Get().GetI32CVar("Atmosphere.Environment.Size"_hsv);
                if (info.CloudsRenderingMode == Passes::Clouds::VP::CloudsRenderingMode::Reprojection)
                    environmentSize *= Passes::Clouds::REPROJECTION_RELATIVE_SIZE;

                passData.ColorOut = graph.Create("CloudsEnvironment"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Format | RGImageInference::Size2d,
                    .Reference = info.ColorIn,
                });

                const Camera camera = Camera::EnvironmentCapture(info.PrimaryView->Camera.Position,
                        (u32)environmentSize, info.FaceIndex);
                ViewInfoGPU viewInfo = *info.PrimaryView;
                viewInfo.Camera = CameraGPU::FromCamera(camera, {environmentSize, environmentSize});
                Resource viewInfoResource = graph.Create("ViewInfo"_hsv, RGBufferDescription{
                    .SizeBytes = sizeof(ViewInfoGPU)});
                viewInfoResource = graph.Upload(viewInfoResource, viewInfo);

                auto& clouds = Passes::Clouds::VP::addToGraph(
                    name.Concatenate(".Clouds").AddVersion(info.FaceIndex), graph, {
                        .ViewInfo = viewInfoResource,
                        .CloudCoverage = info.CloudCoverage,
                        .CloudProfile = info.CloudProfile,
                        .CloudShapeLowFrequencyMap = info.CloudShapeLowFrequencyMap,
                        .CloudShapeHighFrequencyMap = info.CloudShapeHighFrequencyMap,
                        .CloudCurlNoise = info.CloudCurlNoise,
                        .ColorOut = passData.ColorOut,
                        .IrradianceSH = info.IrradianceSH,
                        .Light = info.Light,
                        .CloudParameters = info.CloudParameters,
                        .CloudsRenderingMode = info.CloudsRenderingMode,
                        .IsEnvironmentCapture = true
                    });
                passData.ColorOut = clouds.ColorOut;
            },
            [=](const PassData&, FrameContext&, const Graph&)
            {
            });
    }

    RG::Resource blurComposePass(StringId name, RG::Graph& renderGraph, RG::Resource clouds, RG::Resource atmosphere,
        bool isVerticalBlur)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        struct PassData
        {
            Resource ColorIn{};
            Resource ColorOut{};
        };

        return renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("VP.Environment.BlurCompose.Setup")

                graph.SetShader("cloud-vp-environment-blur-compose"_hsv, ShaderDefines({
                    ShaderDefine{"VERTICAL"_hsv, isVerticalBlur}
                }));

                if (isVerticalBlur)
                    passData.ColorOut = graph.Create("CloudsBlurVertical"_hsv, RGImageDescription{
                        .Inference = RGImageInference::Full,
                        .Reference = clouds,
                    });
                else
                    passData.ColorOut = atmosphere;
                
                passData.ColorIn = graph.ReadImage(clouds, Compute | Sampled);
                passData.ColorOut = graph.ReadWriteImage(passData.ColorOut, Compute | Storage);
            },
            [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("VP.Environment.BlurCompose")
                GPU_PROFILE_FRAME("VP.Environment.BlurCompose")

                const glm::uvec2 resolution = graph.GetImageDescription(passData.ColorOut).Dimensions();

                const Shader& shader = graph.GetShader();
                CloudVpEnvironmentBlurComposeShaderBindGroup bindGroup(shader);
                bindGroup.SetClouds(graph.GetImageBinding(passData.ColorIn));
                bindGroup.SetCloudsAtmosphereBlurred(graph.GetImageBinding(passData.ColorOut));

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(cmd, graph.GetFrameAllocators());
                
                cmd.Dispatch({
                    .Invocations = {resolution.x, resolution.y, 1},
                    .GroupSize = {8, 8, 1}
                });
            }).ColorOut;
    }
}

Passes::Clouds::VP::Environment::PassData& Passes::Clouds::VP::Environment::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            passData = renderPass(name, renderGraph, info);
            Resource atmosphereFace = graph.SplitImage(info.ColorIn,
                {.ImageViewKind = ImageViewKind::Image2d, .LayerBase = (i8)info.FaceIndex, .Layers = 1});
            const Resource blurred = blurComposePass("CloudEnvironmentVerticalBlur"_hsv, graph, passData.ColorOut,
                {}, true);
            atmosphereFace = blurComposePass("CloudEnvironmentHorizontalBlur"_hsv, graph, blurred,
                atmosphereFace, false);
            passData.ColorOut = graph.MergeImage({atmosphereFace});
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}
