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
            resources.ReferenceCommands = passData.BindGroup.SetResourcesCommands(resources.ReferenceCommands);
            resources.RenderObjects = passData.BindGroup.SetResourcesObjects(resources.RenderObjects);
            resources.RenderObjectBuckets = passData.BindGroup.SetResourcesObjectBuckets(resources.RenderObjectBuckets);
            resources.Meshlets = passData.BindGroup.SetResourcesMeshlets(resources.Meshlets);
            resources.MeshletHandles = passData.BindGroup.SetResourcesMeshletsHandles(resources.MeshletHandles);
            resources.ResetMeshletCounts(graph);
            
            if (info.Stage == SceneVisibilityStage::Reocclusion)
            {
                for (u32 i = 0; i < resources.VisibilityCount; i++)
                    if (enumHasAny(multiview.View({i}).ViewInfo.VisibilityFlags(), VisibilityFlags::OcclusionCull))
                        resources.Hiz[i] = passData.BindGroup.SetResourcesHiz(resources.Hiz[i], i);
            }

            for (u32 i = 0; i < resources.VisibilityCount; i++)
            {
                resources.Views[i] = passData.BindGroup.SetResourcesViews(resources.Views[i], i);
                
                resources.RenderObjectVisibility[i] = passData.BindGroup.SetResourcesObjectsVisibility(
                    resources.RenderObjectVisibility[i], i);
                resources.MeshletVisibility[i] = passData.BindGroup.SetResourcesMeshletsVisibilty(
                    resources.MeshletVisibility[i], i);
                resources.MeshletBucketInfos[i] = passData.BindGroup.SetResourcesMeshletInfos(
                    resources.MeshletBucketInfos[i], i);
                resources.MeshletInfoCounts[i] = passData.BindGroup.SetResourcesMeshletInfoCounts(
                    resources.MeshletInfoCounts[i], i);
            }
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("MeshletVisibilityPass")
            GPU_PROFILE_FRAME("MeshletVisibilityPass")

            struct PushConstants
            {
                u32 MeshletCount{0};
                u32 ViewCount{0};
            };
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {PushConstants{
                    .MeshletCount = passData.Resources->MeshletCount,
                    .ViewCount = passData.Resources->VisibilityCount}
                }
            });
            cmd.Dispatch({
               .Invocations = {passData.Resources->MeshletCount, 1, 1},
               .GroupSize = passData.BindGroup.GetMeshletVisibilityGroupSize()
            });
        });
}
