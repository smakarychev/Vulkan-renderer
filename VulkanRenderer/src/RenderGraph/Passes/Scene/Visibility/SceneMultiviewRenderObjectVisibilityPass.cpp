#include "rendererpch.h"

#include "SceneMultiviewRenderObjectVisibilityPass.h"

#include "RenderGraph/Passes/Generated/SceneRenderObjectVisibilityBindGroupRG.generated.h"

Passes::SceneMultiviewRenderObjectVisibility::PassData& Passes::SceneMultiviewRenderObjectVisibility::addToGraph(
    StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, SceneRenderObjectVisibilityBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name, 
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("RenderObjectVisibilityPass.Setup")

            passData.BindGroup = SceneRenderObjectVisibilityBindGroupRG(graph, ShaderSpecializations(
                ShaderSpecialization{"REOCCLUSION"_hsv, info.Stage == SceneVisibilityStage::Reocclusion}));
            
            auto& multiview = *info.MultiviewVisibility;
            
            passData.Resources = info.Resources;
            auto& resources = *passData.Resources;
            resources.RenderObjects = passData.BindGroup.SetResourcesObjects(resources.RenderObjects);
            resources.RenderObjectHandles = passData.BindGroup.SetResourcesObjectHandles(resources.RenderObjectHandles);

            if (info.Stage != SceneVisibilityStage::Reocclusion)
                resources.Init(multiview, graph);

            resources.VisibleRenderObjectsData = passData.BindGroup.SetResourcesVisibleRenderObjects(
                resources.VisibleRenderObjectsData);
            resources.OccludedRenderObjectsData = passData.BindGroup.SetResourcesOccludedRenderObjects(
                resources.OccludedRenderObjectsData);
            resources.VisibilityCountData = passData.BindGroup.SetResourcesVisibilityCountData(
                resources.VisibilityCountData);
            
            for (u32 i = 0; i < resources.VisibilityCount; i++)
                resources.Views[i] = passData.BindGroup.SetResourcesViews(resources.Views[i], i);
            
            for (u32 i = 0; i < resources.VisibilityCount; i++)
            {
                if (!enumHasAny(multiview.View({i}).ViewInfo.VisibilityFlags(), VisibilityFlags::OcclusionCull))
                    continue;
                    
                if (info.Stage == SceneVisibilityStage::Reocclusion)
                    resources.Hiz[i] = passData.BindGroup.SetResourcesHiz(resources.Hiz[i], i);
                else
                    resources.HizPrevious[i] = passData.BindGroup.SetResourcesHizPrevious(resources.HizPrevious[i], i);
            }
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("RenderObjectVisibilityPass")
            GPU_PROFILE_FRAME("RenderObjectVisibilityPass")

            struct PushConstants
            {
                u32 RenderObjectCount{0};
                u32 ViewCount{0};
            };
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {PushConstants{
                    .RenderObjectCount = passData.Resources->RenderObjectCount,
                    .ViewCount = passData.Resources->VisibilityCount
                }}
            });
            cmd.Dispatch({
               .Invocations = {passData.Resources->RenderObjectCount, 1, 1},
               .GroupSize = passData.BindGroup.GetRenderObjectVisibilityGroupSize()
            });
        });
}
