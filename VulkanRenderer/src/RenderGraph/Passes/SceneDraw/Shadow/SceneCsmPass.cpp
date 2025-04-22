#include "SceneCsmPass.h"

#include "SceneDirectionalShadowPass.h"
#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/SceneDraw/SceneDrawPassesCommon.h"
#include "RenderGraph/Passes/Shadows/ShadowPassesUtils.h"
#include "Scene/Visibility/SceneMultiviewVisibility.h"

namespace
{
    std::vector<f32> calculateDepthCascades(const Camera& mainCamera, f32 shadowMin, f32 shadowMax)
    {
        static f32 SPLIT_LAMBDA = 0.0f;
        ImGui::Begin("CSM lambda");
        ImGui::DragFloat("Lambda", &SPLIT_LAMBDA, 1e-2f, 0.0f, 1.0f);
        ImGui::End();

        f32 near = std::max(mainCamera.GetFrustumPlanes().Near, shadowMin);
        f32 far = std::min(mainCamera.GetFrustumPlanes().Far, shadowMax);

        f32 depthRange = far - near;
        f32 depthRatio = far / near;
        
        std::vector depthSplits(SHADOW_CASCADES, 0.0f);
        for (u32 i = 0; i < SHADOW_CASCADES; i++)
        {
            /* https://developer.nvidia.com/gpugems/gpugems3/
             * part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus */
            f32 iOverN = (f32)(i + 1) / (f32)SHADOW_CASCADES;
            f32 cLog = near * std::pow(depthRatio, iOverN);
            f32 cUniform = near + depthRange * iOverN;

            f32 split = Math::lerp(cLog, cUniform, SPLIT_LAMBDA);
            depthSplits[i] = split;
        }

        return depthSplits;
    }

    std::vector<Camera> createShadowCameras(const Camera& mainCamera, const glm::vec3& lightDirection,
        const std::vector<f32>& cascades, f32 shadowMin, const AABB& geometryBounds, bool stabilizeCascades)
    {
        std::vector<Camera> cameras;
        cameras.reserve(SHADOW_CASCADES);
        
        const glm::vec3 up = abs(lightDirection.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
        
        for (u32 i = 0; i < SHADOW_CASCADES; i++)
        {
            f32 previousDepth = i > 0 ? cascades[i - 1] : shadowMin;
            f32 depth = cascades[i];
            FrustumCorners corners = mainCamera.GetFrustumCorners(previousDepth, depth);
            if (stabilizeCascades)
                cameras.push_back(ShadowUtils::shadowCameraStable(corners, geometryBounds, lightDirection, up));
            else
                cameras.push_back(ShadowUtils::shadowCamera(corners, geometryBounds, lightDirection, up));
        }
        
        return cameras;
    }
}

RG::Pass& Passes::SceneCsm::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct Cameras
    {
        std::vector<Camera> ShadowCameras;
    };

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("SceneCsm.Setup")

            Cameras& cameras = graph.GetOrCreateBlackboardValue<Cameras>();
            std::vector cascades = calculateDepthCascades(*info.MainCamera, info.ShadowMin, info.ShadowMax);
            cameras.ShadowCameras =
                createShadowCameras(*info.MainCamera, info.DirectionalLight.Direction, cascades,
                    std::max(info.ShadowMin, info.MainCamera->GetNear()),
                    info.GeometryBounds, info.StabilizeCascades);

            CsmInfo& csmInfo = graph.GetOrCreateBlackboardValue<CsmInfo>();
            csmInfo.CascadeCount = SHADOW_CASCADES;
            std::ranges::copy(cascades.begin(), cascades.end(), csmInfo.Cascades.begin());
            for (u32 i = 0; i < cameras.ShadowCameras.size(); i++)
            {
                auto& camera = cameras.ShadowCameras[i];
                csmInfo.ViewProjections[i] = camera.GetViewProjection();
                csmInfo.Views[i] = camera.GetView();
                csmInfo.Near[i] = camera.GetNear();
                csmInfo.Far[i] = camera.GetFar();
            }

            std::vector<ImageSubresourceDescription> cascadeViews(SHADOW_CASCADES);
            for (u32 i = 0;  i < SHADOW_CASCADES; i++)
                cascadeViews[i] = ImageSubresourceDescription{
                    .MipmapBase = 0, .Mipmaps = 1, .LayerBase = (i8)i, .Layers = 1};
            
            Resource shadow = renderGraph.CreateResource("ShadowMap"_hsv, GraphTextureDescription{
                .Width = SHADOW_MAP_RESOLUTION,
                .Height = SHADOW_MAP_RESOLUTION,
                .Layers = SHADOW_CASCADES,
                .Format = Format::D32_FLOAT,
                .Kind = ImageKind::Image2dArray,
                .AdditionalViews = cascadeViews});

            auto initShadowPassForView = [&](u32 viewIndex)
            {
                return [viewIndex, geometry = info.Geometry](StringId shadowPassName, Graph& shadowGraph,
                    const SceneDrawPassExecutionInfo& shadowInfo)
                    {
                        auto& pass = SceneDirectionalShadow::addToGraph(
                            StringId("{}.DirectionalShadow.{}", shadowPassName, viewIndex),
                            shadowGraph, {
                                .DrawInfo = shadowInfo,
                                .Geometry = geometry});
                        auto& shadowOutput =
                            shadowGraph.GetBlackboard().Get<SceneDirectionalShadow::PassData>(pass);

                        return shadowOutput.Attachments;
                    };
            };

            passData.MetaPassDescriptions.reserve(SHADOW_CASCADES);
            for (u32 i = 0; i < SHADOW_CASCADES; i++)
            {
                DrawAttachments attachments = {
                    .Depth = DepthStencilAttachment{
                        .Resource = shadow,
                        .Description = {
                            .Subresource = cascadeViews[i],
                            .OnLoad = AttachmentLoad::Clear,
                            .ClearDepthStencil = {.Depth = 0.0f, .Stencil = 0}},
                        /* todo: for some reason DEPTH_CONSTANT_BIAS does not do anything at all */
                        .DepthBias = DepthBias{.Constant = DEPTH_CONSTANT_BIAS, .Slope = DEPTH_SLOPE_BIAS}
                    }
                };

                SceneView view = {
                    .Name = StringId("{}.View.{}", name, i),
                    .Camera = &cameras.ShadowCameras[i],
                    .Resolution = glm::uvec2{SHADOW_MAP_RESOLUTION},
                    .VisibilityFlags = SceneVisibilityFlags::ClampDepth | SceneVisibilityFlags::OcclusionCull
                };

                SceneDrawPassDescription description = {
                    .Pass = info.Pass,
                    .DrawPassInit = initShadowPassForView(i),
                    .View = view,
                    .Visibility = info.MultiviewVisibility->AddVisibility(view),
                    .Attachments = attachments
                };

                passData.MetaPassDescriptions.push_back(description);
            }
            
            passData.CsmInfo = graph.CreateResource("CsmInfo"_hsv,
                GraphBufferDescription{.SizeBytes = sizeof(CsmInfo)});
            passData.CsmInfo = graph.Write(passData.CsmInfo, Vertex | Uniform | Copy);
            graph.Upload(passData.CsmInfo, csmInfo);
            
            passData.Near = cameras.ShadowCameras.front().GetFrustumPlanes().Near;
            passData.Far = cameras.ShadowCameras.back().GetFrustumPlanes().Far;

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });
}

ScenePassCreateInfo Passes::SceneCsm::getScenePassCreateInfo(StringId name)
{
    return ScenePassCreateInfo{
        .Name = name,
        .BucketCreateInfos = {
            {
                .Name = "Opaque material"_hsv,
                .Filter = [](const SceneGeometryInfo& geometry, SceneRenderObjectHandle renderObject) {
                    const Material2& material = geometry.MaterialsCpu[
                        geometry.RenderObjects[renderObject.Index].Material];
                    return enumHasAny(material.Flags, MaterialFlags::Opaque);
                },
            }
        },
    };
}
