#include "CSMPass.h"

#include "ShadowPassesCommon.h"
#include "ShadowPassesUtils.h"
#include "imgui/imgui.h"
#include "Light/Light.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/CullMetaMultiviewPass.h"
#include "Rendering/ShaderCache.h"
#include "utils/MathUtils.h"

namespace
{
    std::vector<f32> calculateDepthCascades(const Camera& mainCamera, f32 viewDistance)
    {
        static f32 SPLIT_LAMBDA = 0.0f;
        ImGui::Begin("CSM lambda");
        ImGui::DragFloat("Lambda", &SPLIT_LAMBDA, 1e-2f, 0.0f, 1.0f);
        ImGui::End();

        f32 near = mainCamera.GetFrustumPlanes().Near;
        f32 far = std::min(mainCamera.GetFrustumPlanes().Far, viewDistance);

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

            f32 split = MathUtils::lerp(cLog, cUniform, SPLIT_LAMBDA);
            depthSplits[i] = split;
        }

        return depthSplits;
    }

    std::vector<Camera> createShadowCameras(const Camera& mainCamera, const glm::vec3& lightDirection,
        const std::vector<f32>& cascades, const AABB& geometryBounds)
    {
        f32 near = mainCamera.GetFrustumPlanes().Near;

        /* create shadow cameras */
        std::vector<Camera> cameras;
        cameras.reserve(SHADOW_CASCADES);
        
        glm::vec3 up = abs(lightDirection.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
        for (u32 i = 0; i < SHADOW_CASCADES; i++)
        {
            f32 previousDepth = i > 0 ? cascades[i - 1] : near;
            f32 depth = cascades[i];
            
            FrustumCorners corners = mainCamera.GetFrustumCorners(previousDepth, depth);
            ShadowProjectionBounds bounds = ShadowUtils::projectionBoundsSphereWorld(corners, geometryBounds);

            /* pcss method does not like 0 on a near plane */
            static constexpr f32 NEAR_RELATIVE_OFFSET = 0.1f;

            f32 cameraCentroidOffset = bounds.Min.z * (1.0f + NEAR_RELATIVE_OFFSET);
            glm::vec3 cameraPosition = bounds.Centroid + lightDirection * cameraCentroidOffset;
            Camera shadowCamera = Camera::Orthographic({
                .BaseInfo = {
                    .Position = cameraPosition,
                    .Orientation = glm::normalize(glm::quatLookAt(lightDirection, up)),
                    .Near = -bounds.Min.z * NEAR_RELATIVE_OFFSET,
                    .Far = bounds.Max.z - cameraCentroidOffset,
                    .ViewportWidth = SHADOW_MAP_RESOLUTION,
                    .ViewportHeight = SHADOW_MAP_RESOLUTION},
                .Left = bounds.Min.x,
                .Right = bounds.Max.x,
                .Bottom = bounds.Min.y,
                .Top = bounds.Max.y});

            /* stabilize the camera */
            ShadowUtils::stabilizeShadowProjection(shadowCamera, SHADOW_MAP_RESOLUTION);

            cameras.push_back(shadowCamera);
        }
        
        return cameras;
    }
}

RG::Pass& Passes::CSM::addToGraph(std::string_view name, RG::Graph& renderGraph, const ShadowPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    //todo: this should be shader between all multiview passes obv.
    struct Multiview
    {
        CullMultiviewData MultiviewData{};
    };

    struct Cameras
    {
        std::vector<Camera> ShadowCameras;
    };
    struct Ubo
    {
        u32 CascadeCount{0};
        std::array<f32, MAX_SHADOW_CASCADES> Cascades{};
        std::array<glm::mat4, MAX_SHADOW_CASCADES> ViewProjections{};
        std::array<glm::mat4, MAX_SHADOW_CASCADES> Views{};
        std::array<f32, MAX_SHADOW_CASCADES> Near{};
        std::array<f32, MAX_SHADOW_CASCADES> Far{};
    };
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("CSM.Setup")
            
            if (!graph.TryGetBlackboardValue<Multiview>())
            {
                Multiview& multiview = graph.GetOrCreateBlackboardValue<Multiview>();

                for (u32 i = 0; i < SHADOW_CASCADES; i++)
                    multiview.MultiviewData.AddView({
                        .Geometry = info.Geometry,
                        .DrawShader = &ShaderCache::Register(std::format("{}.{}", name, i),
                            "../assets/shaders/shadow.shader", {}),
                        .DrawTrianglesShader = &ShaderCache::Register(std::format("{}.{}.Triangles", name, i),
                            "../assets/shaders/shadow.shader", 
                            ShaderOverrides{}
                                .Add({"COMPOUND_INDEX"}, true)),
                        .CullTriangles = true}); 

                multiview.MultiviewData.Finalize();
            }

            Cameras& cameras = graph.GetOrCreateBlackboardValue<Cameras>();
            std::vector cascades = calculateDepthCascades(*info.MainCamera, info.ViewDistance);
            cameras.ShadowCameras =
                createShadowCameras(*info.MainCamera, info.DirectionalLight->Direction, cascades, info.GeometryBounds);

            Ubo& ubo = graph.GetOrCreateBlackboardValue<Ubo>();
            ubo.CascadeCount = SHADOW_CASCADES;
            std::ranges::copy(cascades.begin(), cascades.end(), ubo.Cascades.begin());
            for (u32 i = 0; i < cameras.ShadowCameras.size(); i++)
            {
                auto& camera = cameras.ShadowCameras[i];
                ubo.ViewProjections[i] = camera.GetViewProjection();
                ubo.Views[i] = camera.GetView();
                ubo.Near[i] = camera.GetNear();
                ubo.Far[i] = camera.GetFar();
            }

            std::vector<ImageSubresourceDescription::Packed> cascadeViews(SHADOW_CASCADES);
            for (u32 i = 0;  i < SHADOW_CASCADES; i++)
                cascadeViews[i] = ImageSubresourceDescription::Pack({
                    .MipmapBase = 0, .Mipmaps = 1, .LayerBase = i, .Layers = 1});
            
            Resource shadow = renderGraph.CreateResource("CSM.ShadowMap", GraphTextureDescription{
                .Width = SHADOW_MAP_RESOLUTION,
                .Height = SHADOW_MAP_RESOLUTION,
                .Layers = SHADOW_CASCADES,
                .Format = Format::D32_FLOAT,
                .Kind = ImageKind::Image2dArray,
                .AdditionalViews = cascadeViews});

            auto& multiview = graph.GetBlackboardValue<Multiview>();
            for (u32 i = 0; i < SHADOW_CASCADES; i++)
                multiview.MultiviewData.UpdateView(i, {
                    .Resolution = glm::uvec2{SHADOW_MAP_RESOLUTION},
                    .Camera = &cameras.ShadowCameras[i],
                    .ClampDepth = true,
                    .DrawInfo = {
                        .Attachments = {
                            .Depth = DepthStencilAttachment{
                                .Resource = shadow,
                                .Description = {
                                    .Subresource = cascadeViews[i],
                                    .OnLoad = AttachmentLoad::Clear,
                                    .ClearDepth = 0.0f,
                                    .ClearStencil = 0},
                                /* todo: for some reason DEPTH_CONSTANT_BIAS does not do anything at all */
                                .DepthBias = DepthBias{.Constant = DEPTH_CONSTANT_BIAS, .Slope = DEPTH_SLOPE_BIAS}}}}});

            auto& meta = Meta::CullMultiview::addToGraph(std::format("{}.Meta", name), renderGraph,
                multiview.MultiviewData);
            auto& metaOutput = renderGraph.GetBlackboard().Get<Meta::CullMultiview::PassData>(meta);

            passData.CSM = graph.CreateResource("CSM.Data", GraphBufferDescription{.SizeBytes = sizeof(Ubo)});
            passData.CSM = graph.Write(passData.CSM, Vertex | Uniform | Upload);
            passData.ShadowMap = *metaOutput.DrawAttachmentResources.back().Depth;
            passData.Far = cameras.ShadowCameras.back().GetFrustumPlanes().Far;
            
            renderGraph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            auto& ubo = resources.GetOrCreateValue<Ubo>();
            resources.GetBuffer(passData.CSM, ubo, *frameContext.ResourceUploader);
        });

    return pass;
}
