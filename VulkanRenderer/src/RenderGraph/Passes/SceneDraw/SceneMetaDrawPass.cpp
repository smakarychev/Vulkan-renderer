#include "rendererpch.h"

#include "SceneMetaDrawPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewCreateDrawCommandsPass.h"
#include "Scene/SceneRenderObjectSet.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewMeshletVisibilityPass.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewRenderObjectVisibilityPass.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneMultiviewVisibilityHiZPass.h"
#include "RenderGraph/Passes/Scene/Visibility/SceneVisibilityExpandMeshletsPass.h"

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

            const u32 bucketCount = info.MultiviewVisibility->ObjectSet().BucketCount();
            std::array<Resource, SceneMultiviewVisibility::MAX_VIEWS> passDepths{};

            auto drawWithVisibility = [&](u32 visibilityIndex, bool reocclusion)
            {
                for (auto& pass : info.DrawPasses)
                {
                    if (info.MultiviewVisibility->VisibilityHandleToIndex(pass.Visibility) != visibilityIndex)
                        continue;

                    const SceneView& mainVisibilityView = info.MultiviewVisibility->View(pass.Visibility);
                    
                    DrawAttachments& inputAttachments = passData.DrawPassViewAttachments.Get(
                        pass.SceneView.Name, pass.Pass->Name());
                    
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
                        u32 drawIndex = visibilityIndex * bucketCount + bucketIndex;
                        
                        attachmentResources = pass.DrawPassInit(
                            StringId("{}.{}.{}.{}.{}",
                                name, pass.SceneView.Name, pass.Pass->Name(), bucket.Name(),
                                reocclusion ? "Reocclusion" : ""),
                            graph, {
                                .Draws = info.Resources->Draws[drawIndex],
                                .DrawInfo = info.Resources->DrawInfos[drawIndex],
                                .ViewInfo = info.Resources->Views[visibilityIndex],
                                .VisibleMeshlets = info.Resources->VisibleMeshletsData,
                                .Attachments = inputAttachments,
                                .BucketOverrides = &bucket.ShaderOverrides
                            });
                        
                        auto&& [colors, depth] = attachmentResources;

                        if (depth.has_value())
                            if (pass.SceneView == mainVisibilityView)
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
                passData.DrawPassViewAttachments.Add(viewPass.SceneView.Name, viewPass.Pass->Name(), viewPass.Attachments);

            u64 bucketsMask = 0;
            for (auto& pass : info.DrawPasses)
            {
                for (auto& bucketHandle : pass.Pass->BucketHandles())
                {
                    const u32 bucketIndex = info.MultiviewVisibility->ObjectSet().BucketHandleToIndex(bucketHandle);
                    bucketsMask |= (u64)1 << bucketIndex;
                }
            }
        
            SceneMultiviewRenderObjectVisibility::addToGraph(
                name.Concatenate("SceneROVisibility"),
                graph, {
                    .MultiviewVisibility = info.MultiviewVisibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Cull
                });
            SceneVisibilityExpandMeshlets::addToGraph(
                name.Concatenate(".ExpandMeshlets"),
                graph, {
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Cull,
                });
            SceneMultiviewMeshletVisibility::addToGraph(
                name.Concatenate("SceneMeshletVisibility"),
                graph, {
                    .MultiviewVisibility = info.MultiviewVisibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Cull
                });

            SceneMultiviewCreateDrawCommands::addToGraph(
                name.Concatenate("CreateDrawCommands"),
                graph, {
                    .MultiviewVisibility = info.MultiviewVisibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Cull,
                    .BucketsMask = bucketsMask
                });
            for (u32 visibilityIndex = 0; visibilityIndex < info.MultiviewVisibility->VisibilityCount();
                visibilityIndex++)
                drawWithVisibility(visibilityIndex, /*reocclusion*/false);

            SceneMultiviewVisibilityHiz::addToGraph(
                name.Concatenate("HizMultiview"),
                graph, {
                    .MultiviewVisibility = info.MultiviewVisibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Cull,
                    .Depths = passDepths
                });
            
            SceneMultiviewRenderObjectVisibility::addToGraph(
                name.Concatenate("SceneROReocclusion"),
                graph, {
                    .MultiviewVisibility = info.MultiviewVisibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Reocclusion
                });
            SceneVisibilityExpandMeshlets::addToGraph(
                name.Concatenate(".ExpandMeshletsReocclusion"),
                graph, {
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Reocclusion,
                });
            SceneMultiviewMeshletVisibility::addToGraph(
                name.Concatenate("SceneMeshletReocclusion"),
                graph, {
                    .MultiviewVisibility = info.MultiviewVisibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Reocclusion
                });
            
            SceneMultiviewCreateDrawCommands::addToGraph(
                name.Concatenate("CreateDrawCommandsReocclusion"),
                graph, {
                    .MultiviewVisibility = info.MultiviewVisibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Reocclusion,
                    .BucketsMask = bucketsMask
                });

            for (u32 visibilityIndex = 0; visibilityIndex < info.MultiviewVisibility->VisibilityCount();
                visibilityIndex++)
                drawWithVisibility(visibilityIndex, /*reocclusion*/true);

            
            SceneMultiviewVisibilityHiz::addToGraph(
                name.Concatenate("HizMultiviewReocclusion"),
                graph, {
                    .MultiviewVisibility = info.MultiviewVisibility,
                    .Resources = info.Resources,
                    .Stage = SceneVisibilityStage::Reocclusion,
                    .Depths = passDepths
                });
            for (auto& pass : info.DrawPasses)
            {
                const u32 visibilityIndex = info.MultiviewVisibility->VisibilityHandleToIndex(pass.Visibility);
                if (info.Resources->MinMaxDepthReductions[visibilityIndex].IsValid())
                    passData.DrawPassViewAttachments.SetMinMaxDepthReduction(
                        info.MultiviewVisibility->View(pass.Visibility).Name,
                        info.Resources->MinMaxDepthReductions[visibilityIndex]);
            }

        },
        [=](const PassData&, FrameContext&, const Graph&)
        {
        });
}
