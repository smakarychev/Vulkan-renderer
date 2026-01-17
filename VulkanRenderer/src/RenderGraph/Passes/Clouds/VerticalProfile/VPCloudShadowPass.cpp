#include "rendererpch.h"

#include "VPCloudShadowPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/Passes/Generated/CloudsVPShadowBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/CloudsVPShadowBlurBindGroupRG.generated.h"
#include "RenderGraph/Passes/Shadows/ShadowPassesUtils.h"
#include "Scene/SceneLight.h"

namespace 
{
RG::Resource blurPass(StringId name, RG::Graph& renderGraph, RG::Resource shadow, bool isVerticalBlur)
{
    using namespace RG;
    struct PassData
    {
        Resource Blurred{};
    };
    using PassDataBind = PassDataWithBind<PassData, CloudsVPShadowBlurBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("VP.Cloud.ShadowBlur.Setup")

            passData.BindGroup = CloudsVPShadowBlurBindGroupRG(graph, ShaderDefines({
                ShaderDefine{"VERTICAL"_hsv, isVerticalBlur}
            }));

            passData.Blurred = passData.BindGroup.SetResourcesShadowBlurred(graph.Create("Shadow.Blurred"_hsv,
                RGImageDescription{
                    .Inference = RGImageInference::Full,
                    .Reference = shadow,
            }));

            passData.BindGroup.SetResourcesShadow(shadow);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("VP.Cloud.ShadowBlur")
            GPU_PROFILE_FRAME("VP.Cloud.ShadowBlur")

            const glm::uvec2 resolution = graph.GetImageDescription(passData.Blurred).Dimensions();

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = passData.BindGroup.GetCloudsVPShadowBlurGroupSize()
            });
        }).Blurred;
}

Passes::Clouds::VP::Shadow::PassData& renderShadowsPass(StringId name, RG::Graph& renderGraph,
    const Passes::Clouds::VP::Shadow::ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<Passes::Clouds::VP::Shadow::PassData, CloudsVPShadowBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("VP.Cloud.Shadow.Setup")

            passData.BindGroup = CloudsVPShadowBindGroupRG(graph);

            ViewInfoGPU viewInfo = *info.PrimaryView;
            const Camera& primaryCamera = *info.PrimaryCamera;
            const glm::vec3 lightDirection = info.Light->PositionDirection;
            viewInfo.Camera =
                Passes::Clouds::VP::Shadow::createShadowCamera(primaryCamera, *info.PrimaryView, lightDirection);
            passData.ShadowView = viewInfo;

            passData.Shadow = passData.BindGroup.SetResourcesShadow(graph.Create("Clouds.Depth"_hsv,
                RGImageDescription{
                    .Width = (f32)CVars::Get().GetI32CVar("Clouds.ShadowMap.Size"_hsv).value_or(1),
                    .Height = (f32)CVars::Get().GetI32CVar("Clouds.ShadowMap.Size"_hsv).value_or(1),
                    .Format = Format::R11G11B10,
            }));
            
            passData.BindGroup.SetResourcesView(graph.Upload(graph.Create("ViewInfo"_hsv, RGBufferDescription{
                .SizeBytes = sizeof(ViewInfoGPU)}), viewInfo));
            passData.BindGroup.SetResourcesCoverage(info.CloudCoverage);
            passData.BindGroup.SetResourcesProfile(info.CloudProfile);
            passData.BindGroup.SetResourcesLowFrequency(info.CloudShapeLowFrequencyMap);
            passData.BindGroup.SetResourcesHighFrequency(info.CloudShapeHighFrequencyMap);
            passData.BindGroup.SetResourcesCurlNoise(info.CloudCurlNoise);
            passData.BindGroup.SetResourcesParameters(info.CloudParameters);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("VP.Cloud.Shadow")
            GPU_PROFILE_FRAME("VP.Cloud.Shadow")

            const glm::uvec2 resolution = graph.GetImageDescription(passData.Shadow).Dimensions();

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = passData.BindGroup.GetCloudsVPShadowGroupSize()
            });
        });
}
}

Passes::Clouds::VP::Shadow::PassData& Passes::Clouds::VP::Shadow::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            passData = renderShadowsPass(name, graph, info);
            const Resource blurred = blurPass("CloudShadowVerticalBlur"_hsv, graph, passData.Shadow, true);
            passData.Shadow = blurPass("CloudShadowHorizontalBlur"_hsv, graph, blurred, false);
        },
        [=](const PassData&, FrameContext&, const Graph&){});
}

CameraGPU Passes::Clouds::VP::Shadow::createShadowCamera(const Camera& primaryCamera, const ViewInfoGPU& primaryView,
    const glm::vec3& lightDirection)
{
    static constexpr f32 SNAP = 500.0f;
    static constexpr f32 SHADOW_POS_HEIGHT = 15000.0f;
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
        .Top = cloudsExtent
    });
    ShadowUtils::stabilizeShadowProjection(shadowCamera, resolution);

    return CameraGPU::FromCamera(shadowCamera, glm::uvec2(resolution),
        (VisibilityFlags)primaryView.Camera.VisibilityFlags);
}
