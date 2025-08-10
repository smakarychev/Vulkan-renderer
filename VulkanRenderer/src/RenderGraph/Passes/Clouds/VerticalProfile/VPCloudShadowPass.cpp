#include "VPCloudShadowPass.h"

#include "ViewInfoGPU.h"
#include "VPCloudsPass.h"
#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/CloudVpShadowBindGroup.generated.h"
#include "RenderGraph/Passes/Generated/CloudVpShadowBlurBindGroup.generated.h"
#include "RenderGraph/Passes/Shadows/ShadowPassesUtils.h"
#include "Scene/SceneLight.h"

namespace 
{
    RG::Resource blurPass(StringId name, RG::Graph& renderGraph, RG::Resource shadow, bool isVerticalBlur)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        struct PassData
        {
            Resource Shadow{};
            Resource Blurred{};
        };
        
        return renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("VP.Clouds.ShadowBlur.Setup")

                graph.SetShader("cloud-vp-shadow-blur"_hsv, ShaderSpecializations{
                    ShaderSpecialization{"IS_VERTICAL"_hsv, isVerticalBlur}
                });

                passData.Blurred = graph.Create("Shadow.Blurred"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Full,
                    .Reference = shadow,
                });

                passData.Shadow = graph.ReadImage(shadow, Compute | Sampled);
                passData.Blurred = graph.WriteImage(passData.Blurred, Compute | Storage);
            },
            [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("VP.Clouds.ShadowBlur")
                GPU_PROFILE_FRAME("VP.Clouds.ShadowBlur")

                const glm::uvec2 resolution = graph.GetImageDescription(passData.Shadow).Dimensions();

                const Shader& shader = graph.GetShader();
                CloudVpShadowBlurShaderBindGroup bindGroup(shader);
                bindGroup.SetShadow(graph.GetImageBinding(passData.Shadow));
                bindGroup.SetShadowBlurred(graph.GetImageBinding(passData.Blurred));

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(cmd, graph.GetFrameAllocators());

                cmd.Dispatch({
                    .Invocations = {resolution.x, resolution.y, 1},
                    .GroupSize = {8, 8, 1}
                });
            }).Blurred;
    }

    Passes::Clouds::VP::Shadow::PassData& renderShadowsPass(StringId name, RG::Graph& renderGraph,
        const Passes::Clouds::VP::Shadow::ExecutionInfo& info)
    {
        using namespace RG;
        using enum ResourceAccessFlags;
        using PassData = Passes::Clouds::VP::Shadow::PassData;

        return renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("VP.Clouds.Shadow.Setup")

                graph.SetShader("cloud-vp-shadow"_hsv);

                passData.ViewInfo = graph.Create("ViewInfo"_hsv, RGBufferDescription{
                    .SizeBytes = sizeof(ViewInfoGPU)});
                const Camera& primaryCamera = *info.PrimaryCamera;
                const glm::vec3 lightDirection = info.Light->PositionDirection;
                ViewInfoGPU viewInfo = *info.PrimaryView;
                viewInfo.Camera =
                    Passes::Clouds::VP::Shadow::createShadowCamera(primaryCamera, *info.PrimaryView, lightDirection);
                passData.ViewInfo = graph.Upload(passData.ViewInfo, viewInfo);

                passData.DepthOut = graph.Create("Clouds.Depth"_hsv, RGImageDescription{
                    .Width = (f32)CVars::Get().GetI32CVar("Clouds.ShadowMap.Size"_hsv).value_or(1),
                    .Height = (f32)CVars::Get().GetI32CVar("Clouds.ShadowMap.Size"_hsv).value_or(1),
                    .Format = Format::R11G11B10,
                });

                passData.ViewInfo = graph.ReadBuffer(passData.ViewInfo, Compute | Uniform);
                passData.CloudCoverage = graph.ReadImage(info.CloudCoverage, Compute | Sampled);
                passData.CloudProfile = graph.ReadImage(info.CloudProfile, Compute | Sampled);
                passData.CloudShapeLowFrequencyMap = graph.ReadImage(info.CloudShapeLowFrequencyMap, Compute | Sampled);
                passData.CloudShapeHighFrequencyMap =
                    graph.ReadImage(info.CloudShapeHighFrequencyMap, Compute | Sampled);
                passData.CloudCurlNoise = graph.ReadImage(info.CloudCurlNoise, Compute | Sampled);
                passData.DepthOut = graph.WriteImage(passData.DepthOut, Compute | Storage);
                passData.ShadowCamera = viewInfo.Camera;
            },
            [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("VP.Clouds.Shadow")
                GPU_PROFILE_FRAME("VP.Clouds.Shadow")

                const glm::uvec2 resolution = graph.GetImageDescription(passData.DepthOut).Dimensions();

                const Shader& shader = graph.GetShader();
                CloudVpShadowShaderBindGroup bindGroup(shader);
                bindGroup.SetViewInfo(graph.GetBufferBinding(passData.ViewInfo));
                bindGroup.SetCloudCoverage(graph.GetImageBinding(passData.CloudCoverage));
                bindGroup.SetCloudProfile(graph.GetImageBinding(passData.CloudProfile));
                bindGroup.SetCloudLowFrequency(graph.GetImageBinding(passData.CloudShapeLowFrequencyMap));
                bindGroup.SetCloudHighFrequency(graph.GetImageBinding(passData.CloudShapeHighFrequencyMap));
                bindGroup.SetCloudCurlNoise(graph.GetImageBinding(passData.CloudCurlNoise));
                bindGroup.SetOutBsm(graph.GetImageBinding(passData.DepthOut));

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
}

Passes::Clouds::VP::Shadow::PassData& Passes::Clouds::VP::Shadow::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            passData = renderShadowsPass(name, graph, info);
            const Resource blurred = blurPass("CloudShadowVerticalBlur"_hsv, graph, passData.DepthOut, true);
            passData.DepthOut = blurPass("CloudShadowHorizontalBlur"_hsv, graph, blurred, false);
        },
        [=](const PassData&, FrameContext&, const Graph&){});
}

CameraGPU Passes::Clouds::VP::Shadow::createShadowCamera(const Camera& primaryCamera, const ViewInfoGPU& primaryView,
    const glm::vec3& lightDirection)
{
    static constexpr f32 SNAP = 500.0f;
    static constexpr f32 SHADOW_POS_HEIGHT = 10000.0f;
    static constexpr f32 CLOUDS_PADDING = 100.0f;
    const u32 resolution = (u32)CVars::Get().GetI32CVar("Clouds.ShadowMap.Size"_hsv).value_or(1);
    const f32 cloudsExtent = CVars::Get().GetF32CVar("Clouds.Extent"_hsv).value_or(1);
    const glm::vec3 up = abs(lightDirection.y) < 0.999f ?
        glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
    
    const glm::vec3 cameraPosition =
        glm::floor((primaryCamera.GetPosition() + SNAP * 0.5f) / SNAP) * SNAP  -
        lightDirection * SHADOW_POS_HEIGHT;
    Camera shadowCamera = Camera::Orthographic({
        .BaseInfo = {
            .Position = cameraPosition,
            .Orientation = glm::normalize(glm::quatLookAt(lightDirection, up)),
            .Near = CLOUDS_PADDING,
            .Far = SHADOW_POS_HEIGHT,
            .ViewportWidth = resolution,
            .ViewportHeight = resolution},
        .Left = -cloudsExtent,
        .Right = cloudsExtent,
        .Bottom = -cloudsExtent,
        .Top = cloudsExtent});
    ShadowUtils::stabilizeShadowProjection(shadowCamera, resolution);

    return CameraGPU::FromCamera(shadowCamera, glm::uvec2(resolution), primaryView.Camera.VisibilityFlags);
}
