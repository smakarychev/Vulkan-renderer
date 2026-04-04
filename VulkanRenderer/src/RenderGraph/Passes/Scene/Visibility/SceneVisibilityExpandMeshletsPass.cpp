#include "rendererpch.h"
#include "SceneVisibilityExpandMeshletsPass.h"

#include "RenderGraph/Passes/Generated/SceneVisibilityExpandMeshletsBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/SceneVisibilityExpandMeshletsDispatchesBindGroupRG.generated.h"

namespace Passes::SceneVisibilityExpandMeshlets
{
namespace
{
RG::Resource createIndirectDispatchPass(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    struct PassData
    {
        Resource Dispatch{};
    };
    using PassDataBind = PassDataWithBind<PassData, SceneVisibilityExpandMeshletsDispatchesBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Scene.SceneVisibilityExpandMeshletsDispatch.Setup")

            passData.BindGroup = SceneVisibilityExpandMeshletsDispatchesBindGroupRG(graph, ShaderSpecializations(
                ShaderSpecialization{"REOCCLUSION"_hsv, info.Stage == SceneVisibilityStage::Reocclusion}));

            passData.BindGroup.SetResourcesVisibilityCountData(info.Resources->VisibilityCountData);
            passData.Dispatch =
                passData.BindGroup.SetResourcesCommand(graph.Create("Dispatch"_hsv, RGBufferDescription{
                    .SizeBytes = sizeof(IndirectDispatchCommand)
                }), ResourceAccessFlags::Indirect);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Scene.SceneVisibilityExpandMeshletsDispatch")
            GPU_PROFILE_FRAME("Scene.SceneVisibilityExpandMeshletsDispatch")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
               .Invocations = {1, 1, 1},
               .GroupSize = passData.BindGroup.GetCreateIndirectMeshletDispatchesGroupSize()
            });
        }).Dispatch;
}
}

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    struct PassDataPrivate : PassData
    {
        Resource Dispatch{};
    };
    using PassDataBind = PassDataWithBind<PassDataPrivate, SceneVisibilityExpandMeshletsBindGroupRG>;

    const Resource dispatch = createIndirectDispatchPass(name.Concatenate(".Dispatch"_hsv), renderGraph, info);

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Scene.SceneVisibilityExpandMeshlets.Setup")

            passData.BindGroup = SceneVisibilityExpandMeshletsBindGroupRG(graph, ShaderSpecializations(
                ShaderSpecialization{"REOCCLUSION"_hsv, info.Stage == SceneVisibilityStage::Reocclusion}));

            passData.Resources = info.Resources;
            auto& resources = *passData.Resources;
            
            resources.RenderObjects = passData.BindGroup.SetResourcesRenderObjects(resources.RenderObjects);
            resources.RenderObjectHandles = passData.BindGroup.SetResourcesObjectHandles(resources.RenderObjectHandles);
            passData.BindGroup.SetResourcesVisibleRenderObjects(resources.VisibleRenderObjectsData);
            resources.ExpandedMeshlets =
                passData.BindGroup.SetResourcesExpandedMeshlets(resources.ExpandedMeshlets);
            resources.VisibilityCountData =
                passData.BindGroup.SetResourcesVisibilityCountData(resources.VisibilityCountData);

            passData.Dispatch = graph.ReadBuffer(dispatch, ResourceAccessFlags::Indirect);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.SceneVisibilityExpandMeshlets")
            GPU_PROFILE_FRAME("Scene.SceneVisibilityExpandMeshlets")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.DispatchIndirect({
                .Buffer = graph.GetBuffer(passData.Dispatch)
            });
        });
}
}
