#include "SceneMetaDrawPass.h"

#include <ranges>

#include "SceneFillIndirectDrawPass.h"
#include "RenderGraph/RenderGraph.h"
#include "Scene/SceneRenderObjectSet.h"
#include "Visibility/SceneMultiviewMeshletVisibilityPass.h"
#include "Visibility/SceneMultiviewRenderObjectVisibilityPass.h"
#include "Visibility/SceneMultiviewVisibilityHiZPass.h"

RG::Pass& Passes::SceneMetaDraw::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("SceneMetaDraw")
            
            std::unordered_map<const SceneView*, u32> viewToViewIndex{};
            viewToViewIndex.reserve(SceneMultiviewVisibility::MAX_VIEWS);
            for (u32 viewIndex = 0; viewIndex < info.Visibility->ViewCount(); viewIndex++)
                viewToViewIndex.emplace(&info.Visibility->Visibilities()[viewIndex]->GetView(), viewIndex);
            std::array<SceneFillIndirectDraw::PassData, SceneMultiviewVisibility::MAX_VIEWS> bucketDrawInfos{};
            std::array<Resource, SceneMultiviewVisibility::MAX_VIEWS> passDepths{};
            std::array<ImageSubresourceDescription, SceneMultiviewVisibility::MAX_VIEWS> passDepthsSubresources{};

            auto fillBuckets = [&](u32 viewIndex, bool reocclusion)
            {
                auto& fillIndirectDraws = SceneFillIndirectDraw::addToGraph(
                    StringId("{}.FillSceneIndirectDraws.{}.{}", name, viewIndex, reocclusion ? "Reocclusion" : ""),
                    graph, {
                        .Geometry = &info.Visibility->ObjectSet()->Geometry(),
                        .RenderObjectSet = info.Visibility->ObjectSet(),
                        .MeshletInfos = info.Resources->MeshletBucketInfos[viewIndex],
                        .MeshletInfoCount = info.Resources->MeshletInfoCounts[viewIndex]});
                auto& fillIndirectDrawsOutput =
                    graph.GetBlackboard().Get<SceneFillIndirectDraw::PassData>(fillIndirectDraws);
                bucketDrawInfos[viewIndex] = fillIndirectDrawsOutput;
            };
            auto drawView = [&](auto& viewPass, bool reocclusion)
            {
                auto& view = *viewPass.View;
                const u32 viewIndex = viewToViewIndex.at(&view);
                auto& drawInfo = bucketDrawInfos[viewIndex];

                DrawAttachments inputAttachments = viewPass.Attachments;
                if (reocclusion)
                {
                    for (auto& color : inputAttachments.Colors)
                        color.Description.OnLoad = AttachmentLoad::Load;
                    if (inputAttachments.Depth.has_value())
                        inputAttachments.Depth->Description.OnLoad = AttachmentLoad::Load;
                }

                if (viewPass.BucketsPasses.empty())
                    return;

                for (u32 infoIndex = 0; infoIndex < viewPass.BucketsPasses.size(); infoIndex++)
                {
                    auto& passInfo = viewPass.BucketsPasses[infoIndex];
                    
                    const u32 bucketIndex =
                        info.Visibility->ObjectSet()->BucketHandleToIndex(passInfo.Bucket->Handle());
                    auto&& [_, attachments] = passInfo.DrawPassInit(
                        StringId("{}.{}.{}", name, viewIndex, reocclusion ? "Reocclusion" : ""),
                        graph, {
                            .Draws = drawInfo.Draws[bucketIndex],
                            .DrawInfo = drawInfo.DrawInfos[bucketIndex],
                            .Resolution = view.Resolution,
                            .Camera = view.Camera,
                            .Attachments = inputAttachments});

                    for (u32 i = 0; i < attachments.Colors.size(); i++)
                        inputAttachments.Colors[i].Resource = attachments.Colors[i];
                    if (attachments.Depth.has_value())
                    {
                        passDepths[viewIndex] = attachments.Depth.value_or(Resource{});
                        passDepthsSubresources[viewIndex] = inputAttachments.Depth.has_value() ?
                            inputAttachments.Depth->Description.Subresource : ImageSubresourceDescription{};
                        inputAttachments.Depth->Resource = *attachments.Depth;
                    }

                    if (!reocclusion && infoIndex == 1)
                    {
                        for (auto& color : inputAttachments.Colors)
                            color.Description.OnLoad = AttachmentLoad::Load;
                        if (inputAttachments.Depth.has_value())
                            inputAttachments.Depth->Description.OnLoad = AttachmentLoad::Load;
                    }
                }
            };
            
            SceneMultiviewRenderObjectVisibility::addToGraph(
                name.Concatenate("SceneROVisibility"),
                graph, {
                    .Visibility = info.Visibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Cull});
            SceneMultiviewMeshletVisibility::addToGraph(
                name.Concatenate("SceneMeshletVisibility"),
                graph, {
                    .Visibility = info.Visibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Cull});

            for (u32 viewIndex = 0; viewIndex < info.Visibility->ViewCount(); viewIndex++)
                fillBuckets(viewIndex, /*reocclusion*/false);

            for (auto& viewPass : info.ViewPasses)
                drawView(viewPass, /*reocclusion*/false);

            SceneMultiviewVisibilityHiz::addToGraph(
                name.Concatenate("HizMultiview"),
                graph, {
                    .Visibility = info.Visibility,
                    .Resources = info.Resources,
                    .Depths = passDepths,
                    .Subresources = passDepthsSubresources});

            SceneMultiviewRenderObjectVisibility::addToGraph(
                name.Concatenate("SceneROReocclusion"),
                graph, {
                    .Visibility = info.Visibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Reocclusion});
            SceneMultiviewMeshletVisibility::addToGraph(
                name.Concatenate("SceneMeshletReocclusion"),
                graph, {
                    .Visibility = info.Visibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Reocclusion});

            for (u32 viewIndex = 0; viewIndex < info.Visibility->ViewCount(); viewIndex++)
                fillBuckets(viewIndex, /*reocclusion*/true);

            for (auto& viewPass : info.ViewPasses)
                drawView(viewPass, /*reocclusion*/true);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            
        });
}
