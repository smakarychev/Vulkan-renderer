#include "SceneMetaDrawPass.h"

#include "SceneFillIndirectDrawPass.h"
#include "RenderGraph/RGGraph.h"
#include "Scene/SceneRenderObjectSet.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewMeshletVisibilityPass.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewRenderObjectVisibilityPass.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewVisibilityHiZPass.h"

Passes::SceneMetaDraw::PassData& Passes::SceneMetaDraw::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    // todo: this should do something else if no pass uses occlusion culling, or if hiz info already available
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("SceneMetaDraw")

            std::array<Resource, SceneMultiviewVisibility::MAX_VIEWS> passDepths{};
            std::array<Resource, MAX_BUCKETS_PER_SET> draws{};
            std::array<Resource, MAX_BUCKETS_PER_SET> drawInfos{};
            u32 maxBucketIndex = 0;

            auto drawWithVisibility = [&](u32 visibilityIndex, bool reocclusion)
            {
                auto& fillIndirectDraws = SceneFillIndirectDraw::addToGraph(
                    StringId("{}.FillSceneIndirectDraws.{}.{}", name, visibilityIndex, reocclusion ? "Reocclusion" : ""),
                    graph, {
                        .Geometry = &info.MultiviewVisibility->ObjectSet().Geometry(),
                        .Draws = draws,
                        .DrawInfos = drawInfos,
                        .BucketCount = maxBucketIndex + 1,
                        .MeshletInfos = info.Resources->MeshletBucketInfos[visibilityIndex],
                        .MeshletInfoCount = info.Resources->MeshletInfoCounts[visibilityIndex]});

                draws = fillIndirectDraws.Draws;
                drawInfos = fillIndirectDraws.DrawInfos;

                for (auto& pass : info.DrawPasses)
                {
                    if (info.MultiviewVisibility->VisibilityHandleToIndex(pass.Visibility) != visibilityIndex)
                        continue;

                    const SceneView& mainVisibilityView = info.MultiviewVisibility->View(pass.Visibility);
                    
                    DrawAttachments& inputAttachments = passData.DrawPassViewAttachments.Get(
                        pass.View.Name, pass.Pass->Name());
                    
                    for (auto& color : inputAttachments.Colors)
                        ASSERT(color.Resource.HasFlags(ResourceFlags::AutoUpdate),
                            "SceneMetaDraw pass expects all attachments to be created with 'AutoUpdate' flag")
                    if (inputAttachments.Depth)
                        ASSERT(inputAttachments.Depth->Resource.HasFlags(ResourceFlags::AutoUpdate),
                            "SceneMetaDraw pass expects all attachments to be created with 'AutoUpdate' flag")
                    
                    if (reocclusion)
                    {
                        for (auto& color : inputAttachments.Colors)
                            color.Description.OnLoad = AttachmentLoad::Load;
                        if (inputAttachments.Depth.has_value())
                            inputAttachments.Depth->Description.OnLoad = AttachmentLoad::Load;
                    }

                    DrawAttachmentResources attachmentResources = {};
                    for (auto&& [handleIndex, bucketHandle] : std::views::enumerate(pass.Pass->BucketHandles()))
                    {
                        const u32 bucketIndex = info.MultiviewVisibility->ObjectSet().BucketHandleToIndex(bucketHandle);
                        auto& bucket = pass.Pass->BucketFromHandle(bucketHandle);
                                    
                        attachmentResources = pass.DrawPassInit(
                            StringId("{}.{}.{}.{}.{}",
                                name, pass.View.Name, pass.Pass->Name(), bucket.Name(),
                                reocclusion ? "Reocclusion" : ""),
                            graph, {
                                .Draws = draws[bucketIndex],
                                .DrawInfo = drawInfos[bucketIndex],
                                .Resolution = pass.View.Resolution,
                                .Camera = pass.View.Camera,
                                .Attachments = inputAttachments,
                                .BucketOverrides = &bucket.ShaderOverrides});
                        
                        auto&& [colors, depth] = attachmentResources;

                        if (depth.has_value())
                            if (pass.View == mainVisibilityView)
                                passDepths[visibilityIndex] = depth.value_or(Resource{});

                        if (!reocclusion && handleIndex == 0)
                        {
                            for (auto& color : inputAttachments.Colors)
                                color.Description.OnLoad = AttachmentLoad::Load;
                            if (inputAttachments.Depth.has_value())
                                inputAttachments.Depth->Description.OnLoad = AttachmentLoad::Load;
                        }
                    }
                }
            };
            
            for (auto& viewPass : info.DrawPasses)
                passData.DrawPassViewAttachments.Add(viewPass.View.Name, viewPass.Pass->Name(), viewPass.Attachments);
            
            for (auto& pass : info.DrawPasses)
            {
                for (auto& bucketHandle : pass.Pass->BucketHandles())
                {
                    const u32 bucketIndex = info.MultiviewVisibility->ObjectSet().BucketHandleToIndex(bucketHandle);
                    auto& bucket = pass.Pass->BucketFromHandle(bucketHandle);

                    draws[bucketIndex] = graph.Import(
                        StringId("Draw"_hsv).AddVersion(bucketIndex),
                        bucket.Draws());
                    drawInfos[bucketIndex] = graph.Import(
                        StringId("DrawInfo"_hsv).AddVersion(bucketIndex),
                        bucket.DrawInfo());
                    maxBucketIndex = std::max(maxBucketIndex, bucketIndex);
                }
            }
        
            SceneMultiviewRenderObjectVisibility::addToGraph(
                name.Concatenate("SceneROVisibility"),
                graph, {
                    .MultiviewVisibility = info.MultiviewVisibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Cull});
            SceneMultiviewMeshletVisibility::addToGraph(
                name.Concatenate("SceneMeshletVisibility"),
                graph, {
                    .MultiviewVisibility = info.MultiviewVisibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Cull});

            for (u32 visibilityIndex = 0; visibilityIndex < info.MultiviewVisibility->VisibilityCount();
                visibilityIndex++)
                drawWithVisibility(visibilityIndex, /*reocclusion*/false);

            SceneMultiviewVisibilityHiz::addToGraph(
                name.Concatenate("HizMultiview"),
                graph, {
                    .MultiviewVisibility = info.MultiviewVisibility,
                    .Resources = info.Resources,
                    .Depths = passDepths});
            for (auto& pass : info.DrawPasses)
            {
                const u32 visibilityIndex = info.MultiviewVisibility->VisibilityHandleToIndex(pass.Visibility);
                if (info.Resources->MinMaxDepthReductions[visibilityIndex].IsValid())
                    passData.DrawPassViewAttachments.SetMinMaxDepthReduction(
                        info.MultiviewVisibility->View(pass.Visibility).Name,
                        info.Resources->MinMaxDepthReductions[visibilityIndex]);
            }

            SceneMultiviewRenderObjectVisibility::addToGraph(
                name.Concatenate("SceneROReocclusion"),
                graph, {
                    .MultiviewVisibility = info.MultiviewVisibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Reocclusion});
            SceneMultiviewMeshletVisibility::addToGraph(
                name.Concatenate("SceneMeshletReocclusion"),
                graph, {
                    .MultiviewVisibility = info.MultiviewVisibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Reocclusion});

            for (u32 visibilityIndex = 0; visibilityIndex < info.MultiviewVisibility->VisibilityCount();
                visibilityIndex++)
                drawWithVisibility(visibilityIndex, /*reocclusion*/true);
        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}
