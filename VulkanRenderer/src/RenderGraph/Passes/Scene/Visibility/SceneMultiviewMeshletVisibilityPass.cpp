#include "rendererpch.h"

#include "SceneMultiviewMeshletVisibilityPass.h"

#include "RenderGraph/Passes/Generated/SceneMeshletVisibilityBindGroupRG.generated.h"

Passes::SceneMultiviewMeshletVisibility::PassData& Passes::SceneMultiviewMeshletVisibility::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, SceneMeshletVisibilityBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("MeshletVisibilityPass.Setup")

            passData.BindGroup = SceneMeshletVisibilityBindGroupRG(graph, ShaderSpecializations(
                ShaderSpecialization{"REOCCLUSION"_hsv, info.Stage == SceneVisibilityStage::Reocclusion}));

            auto& multiview = *info.MultiviewVisibility;
            
            passData.Resources = info.Resources;
            auto& resources = *passData.Resources;
            resources.Meshlets = passData.BindGroup.SetResourcesMeshletsUgb(resources.Meshlets);
            resources.RenderObjects = passData.BindGroup.SetResourcesObjects(resources.RenderObjects);
            
            resources.ExpandedMeshlets =
                passData.BindGroup.SetResourcesExpandedMeshlets(resources.ExpandedMeshlets);
            resources.VisibleMeshletsData =
                passData.BindGroup.SetResourcesVisibleMeshlets(resources.VisibleMeshletsData);
            resources.OccludedMeshletsData =
                passData.BindGroup.SetResourcesOccludedMeshlets(resources.OccludedMeshletsData);
            resources.VisibilityCountData = passData.BindGroup.SetResourcesVisibilityCountData(
                resources.VisibilityCountData);
            
            for (u32 i = 0; i < resources.VisibilityCount; i++)
            {
                if (!enumHasAny(multiview.View({i}).ViewInfo.VisibilityFlags(), VisibilityFlags::OcclusionCull))
                    continue;
                    
                if (info.Stage == SceneVisibilityStage::Reocclusion)
                    resources.Hiz[i] = passData.BindGroup.SetResourcesHiz(resources.Hiz[i], i);
                else
                    resources.HizPrevious[i] = passData.BindGroup.SetResourcesHizPrevious(resources.HizPrevious[i], i);
            }

            for (u32 i = 0; i < resources.VisibilityCount; i++)
                resources.Views[i] = passData.BindGroup.SetResourcesViews(resources.Views[i], i);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("MeshletVisibilityPass")
            GPU_PROFILE_FRAME("MeshletVisibilityPass")

            struct PushConstants
            {
                u32 ViewCount{0};
            };
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {PushConstants{
                    .ViewCount = passData.Resources->VisibilityCount}
                }
            });
            cmd.Dispatch({
               .Invocations = {passData.Resources->MeshletCount, 1, 1},
               .GroupSize = passData.BindGroup.GetMeshletVisibilityGroupSize()
            });
        });
}
