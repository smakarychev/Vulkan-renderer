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

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("SceneMetaDraw")

            std::array<SceneFillIndirectDraw::PassData, SceneMultiviewVisibility::MAX_VIEWS> passDrawInfos{};
            std::array<Resource, SceneMultiviewVisibility::MAX_VIEWS> passDepths{};
            std::array<ImageSubresourceDescription, SceneMultiviewVisibility::MAX_VIEWS> passDepthsSubresources{};

            auto drawWithVisibility = [&](u32 visibilityIndex, bool reocclusion)
            {
                auto& fillIndirectDraws = SceneFillIndirectDraw::addToGraph(
                    StringId("{}.FillSceneIndirectDraws.{}.{}", name, visibilityIndex, reocclusion ? "Reocclusion" : ""),
                    graph, {
                        .Geometry = &info.MultiviewVisibility->ObjectSet().Geometry(),
                        .RenderObjectSet = &info.MultiviewVisibility->ObjectSet(),
                        .MeshletInfos = info.Resources->MeshletBucketInfos[visibilityIndex],
                        .MeshletInfoCount = info.Resources->MeshletInfoCounts[visibilityIndex]});
                auto& fillIndirectDrawsOutput =
                    graph.GetBlackboard().Get<SceneFillIndirectDraw::PassData>(fillIndirectDraws);
                passDrawInfos[visibilityIndex] = fillIndirectDrawsOutput;

                for (auto& pass : info.DrawPasses)
                {
                    if (info.MultiviewVisibility->VisibilityHandleToIndex(pass.Visibility) != visibilityIndex)
                        continue;

                    const SceneView& mainVisibilityView = info.MultiviewVisibility->View(pass.Visibility);
                    const SceneFillIndirectDraw::PassData& drawInfo = passDrawInfos[visibilityIndex];
                    
                    DrawAttachments& inputAttachments = passData.DrawPassViewAttachments.Get(
                        pass.View.Name, pass.Pass->Name());
                    if (reocclusion)
                    {
                        for (auto& color : inputAttachments.Colors)
                            color.Description.OnLoad = AttachmentLoad::Load;
                        if (inputAttachments.Depth.has_value())
                            inputAttachments.Depth->Description.OnLoad = AttachmentLoad::Load;
                    }

                    for (SceneBucketHandle bucketHandle : pass.Pass->BucketHandles())
                    {
                        const u32 bucketIndex = info.MultiviewVisibility->ObjectSet().BucketHandleToIndex(bucketHandle);
                        auto& bucket = pass.Pass->BucketFromHandle(bucketHandle);
                                    
                        // todo: do the shader specialization for each bucket here:
                        auto [colors, depth] = pass.DrawPassInit(
                            StringId("{}.{}.{}.{}.{}",
                                name, pass.View.Name, pass.Pass->Name(), bucket.Name(),
                                reocclusion ? "Reocclusion" : ""),
                            graph, {
                                .Draws = drawInfo.Draws[bucketIndex],
                                .DrawInfo = drawInfo.DrawInfos[bucketIndex],
                                .Resolution = pass.View.Resolution,
                                .Camera = pass.View.Camera,
                                .Attachments = inputAttachments,
                                .Overrides = &bucket.ShaderOverrides});

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

                        if (!reocclusion && bucketIndex == 0)
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
