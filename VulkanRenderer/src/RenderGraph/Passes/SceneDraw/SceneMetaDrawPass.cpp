#include "SceneMetaDrawPass.h"

#include "SceneFillIndirectDrawPass.h"
#include "RenderGraph/RenderGraph.h"
#include "Scene/SceneRenderObjectSet.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewMeshletVisibilityPass.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewRenderObjectVisibilityPass.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewVisibilityHiZPass.h"

RG::Pass& Passes::SceneMetaDraw::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    // todo: this should do something else if no pass uses occlusion culling, or if hiz info already available
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("SceneMetaDraw")

            std::array<Resource, SceneMultiviewVisibility::MAX_VIEWS> passDepths{};
            std::array<ImageSubresourceDescription, SceneMultiviewVisibility::MAX_VIEWS> passDepthsSubresources{};
            std::array<Resource, MAX_BUCKETS_PER_SET> draws;
            std::array<Resource, MAX_BUCKETS_PER_SET> drawInfos;

            auto drawWithVisibility = [&](u32 visibilityIndex, bool reocclusion)
            {
                auto& fillIndirectDraws = SceneFillIndirectDraw::addToGraph(
                    StringId("{}.FillSceneIndirectDraws.{}.{}", name, visibilityIndex, reocclusion ? "Reocclusion" : ""),
                    graph, {
                        .Geometry = &info.MultiviewVisibility->ObjectSet().Geometry(),
                        .Draws = draws,
                        .DrawInfos = drawInfos,
                        .BucketCount = info.MultiviewVisibility->ObjectSet().BucketCount(),
                        .MeshletInfos = info.Resources->MeshletBucketInfos[visibilityIndex],
                        .MeshletInfoCount = info.Resources->MeshletInfoCounts[visibilityIndex]});
                auto& fillIndirectDrawsOutput =
                    graph.GetBlackboard().Get<SceneFillIndirectDraw::PassData>(fillIndirectDraws);

                draws = fillIndirectDrawsOutput.Draws;
                drawInfos = fillIndirectDrawsOutput.DrawInfos;

                for (auto& pass : info.DrawPasses)
                {
                    if (info.MultiviewVisibility->VisibilityHandleToIndex(pass.Visibility) != visibilityIndex)
                        continue;

                    const SceneView& mainVisibilityView = info.MultiviewVisibility->View(pass.Visibility);
                    
                    DrawAttachments oldAttachments = passData.DrawPassViewAttachments.Get(
                        pass.View.Name, pass.Pass->Name());
                    DrawAttachments& inputAttachments = passData.DrawPassViewAttachments.Get(
                        pass.View.Name, pass.Pass->Name());
                    if (reocclusion)
                    {
                        for (auto& color : inputAttachments.Colors)
                            color.Description.OnLoad = AttachmentLoad::Load;
                        if (inputAttachments.Depth.has_value())
                            inputAttachments.Depth->Description.OnLoad = AttachmentLoad::Load;
                    }

                    DrawAttachmentResources attachmentResources = {};
                    for (auto&& [handleIndex, bucketHandle] : std::ranges::views::enumerate(pass.Pass->BucketHandles()))
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

                        for (u32 i = 0; i < colors.size(); i++)
                            inputAttachments.Colors[i].Resource = colors[i];

                        if (depth.has_value())
                        {
                            if (pass.View == mainVisibilityView)
                            {
                                passDepths[visibilityIndex] = depth.value_or(Resource{});
                                passDepthsSubresources[visibilityIndex] = inputAttachments.Depth.has_value() ?
                                    inputAttachments.Depth->Description.Subresource : ImageSubresourceDescription{};
                            }
                            inputAttachments.Depth->Resource = *depth;
                        }

                        if (!reocclusion && handleIndex == 0)
                        {
                            for (auto& color : inputAttachments.Colors)
                                color.Description.OnLoad = AttachmentLoad::Load;
                            if (inputAttachments.Depth.has_value())
                                inputAttachments.Depth->Description.OnLoad = AttachmentLoad::Load;
                        }
                    }

                    // todo: remove this garbage >:(
                    passData.DrawPassViewAttachments.UpdateResources(oldAttachments, attachmentResources);
                }
            };
            
            for (auto& viewPass : info.DrawPasses)
                passData.DrawPassViewAttachments.Add(viewPass.View.Name, viewPass.Pass->Name(), viewPass.Attachments);

            u32 bucketIndex = 0;
            for (auto& pass : info.MultiviewVisibility->ObjectSet().Passes())
            {
                for (SceneBucketHandle bucketHandle : pass.BucketHandles())
                {
                    auto& bucket = pass.BucketFromHandle(
                        info.MultiviewVisibility->ObjectSet().BucketHandleToIndex(bucketHandle));
                    draws[bucketIndex] = graph.AddExternal(
                        StringId("Draw"_hsv).AddVersion(bucketIndex),
                        bucket.Draws());
                    drawInfos[bucketIndex] = graph.AddExternal(
                        StringId("DrawInfo"_hsv).AddVersion(bucketIndex),
                        bucket.DrawInfo());
                    bucketIndex++;
                }
            }
            ASSERT(bucketIndex == info.MultiviewVisibility->ObjectSet().BucketCount())
        
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
                    .Depths = passDepths,
                    .Subresources = passDepthsSubresources});

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

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });
}
