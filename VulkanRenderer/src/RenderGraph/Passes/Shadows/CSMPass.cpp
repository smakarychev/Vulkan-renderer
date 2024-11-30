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

            f32 split = MathUtils::lerp(cLog, cUniform, SPLIT_LAMBDA);
            depthSplits[i] = split;
        }

        return depthSplits;
    }

    std::vector<Camera> createShadowCameras(const Camera& mainCamera, const glm::vec3& lightDirection,
        const std::vector<f32>& cascades, f32 shadowMin, const AABB& geometryBounds, bool stabilizeCascades)
    {
        /* create shadow cameras */
        std::vector<Camera> cameras;
        cameras.reserve(SHADOW_CASCADES);
        
        glm::vec3 up = abs(lightDirection.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
        
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
            std::vector cascades = calculateDepthCascades(*info.MainCamera, info.ShadowMin, info.ShadowMax);
            cameras.ShadowCameras =
                createShadowCameras(*info.MainCamera, info.DirectionalLight->Direction, cascades,
                    std::max(info.ShadowMin, info.MainCamera->GetNear()),
                    info.GeometryBounds, info.StabilizeCascades);

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

            std::vector<ImageSubresourceDescription> cascadeViews(SHADOW_CASCADES);
            for (u32 i = 0;  i < SHADOW_CASCADES; i++)
                cascadeViews[i] = ImageSubresourceDescription{
                    .MipmapBase = 0, .Mipmaps = 1, .LayerBase = (u8)i, .Layers = 1};
            
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
            passData.CSM = graph.Write(passData.CSM, Vertex | Uniform | Copy);
            graph.Upload(passData.CSM, ubo);
            
            passData.ShadowMap = *metaOutput.DrawAttachmentResources.back().Depth;
            passData.Far = cameras.ShadowCameras.back().GetFrustumPlanes().Far;
            
            renderGraph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });

    return pass;
}
