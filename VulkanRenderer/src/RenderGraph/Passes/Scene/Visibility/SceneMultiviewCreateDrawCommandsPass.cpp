#include "rendererpch.h"
#include "SceneMultiviewCreateDrawCommandsPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/SceneCreateDrawCommandsBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/SceneCreateDrawCommandsDispatchesBindGroupRG.generated.h"

namespace Passes::SceneMultiviewCreateDrawCommands
{
namespace
{
RG::Resource createDrawCommandsDispatchPass(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    struct PassData
    {
        Resource Dispatch{};
    };
    using PassDataBind = PassDataWithBind<PassData, SceneCreateDrawCommandsDispatchesBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Scene.SceneCreateDrawCommandsDispatch")

            passData.BindGroup = SceneCreateDrawCommandsDispatchesBindGroupRG(graph, ShaderSpecializations(
                ShaderSpecialization{"REOCCLUSION"_hsv, info.Stage == SceneVisibilityStage::Reocclusion}));

            passData.BindGroup.SetResourcesVisibilityCountData(info.Resources->VisibilityCountData);
            passData.Dispatch =
                passData.BindGroup.SetResourcesCommand(graph.Create("Dispatch"_hsv, RGBufferDescription{
                    .SizeBytes = sizeof(IndirectDispatchCommand)
                }), ResourceAccessFlags::Indirect);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Scene.SceneCreateDrawCommandsDispatch")
            GPU_PROFILE_FRAME("Scene.SceneCreateDrawCommandsDispatch")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
               .Invocations = {1, 1, 1},
               .GroupSize = passData.BindGroup.GetCreateDrawCommandsDispatchesGroupSize()
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
    using PassDataBind = PassDataWithBind<PassDataPrivate, SceneCreateDrawCommandsBindGroupRG>;

    const u32 bucketCount = info.MultiviewVisibility->ObjectSet().BucketCount();
    const Resource dispatch = createDrawCommandsDispatchPass(name.Concatenate(".Dispatch"_hsv), renderGraph, info);
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("CreateDrawCommands.Setup")

            passData.BindGroup = SceneCreateDrawCommandsBindGroupRG(graph, ShaderSpecializations(
                ShaderSpecialization{"REOCCLUSION"_hsv, info.Stage == SceneVisibilityStage::Reocclusion}));

            passData.Resources = info.Resources;
            auto& resources = *passData.Resources;
            resources.Meshlets = passData.BindGroup.SetResourcesMeshletsUgb(resources.Meshlets);
            resources.RenderObjects = passData.BindGroup.SetResourcesObjects(resources.RenderObjects);
            resources.RenderObjectBuckets =
                passData.BindGroup.SetResourcesRenderObjectBuckets(resources.RenderObjectBuckets);
            resources.VisibleMeshletsData =
                passData.BindGroup.SetResourcesVisibleMeshlets(resources.VisibleMeshletsData);
            resources.VisibilityCountData = passData.BindGroup.SetResourcesVisibilityCountData(
                resources.VisibilityCountData);

            for (u32 view = 0; view < resources.VisibilityCount; view++)
            {
                for (u32 bucket = 0; bucket < bucketCount; bucket++)
                {
                    const u32 index = view * bucketCount + bucket;
                    resources.Draws[index] = passData.BindGroup.SetResourcesDrawCommands(resources.Draws[index], index);
                    resources.DrawInfos[index] =
                        passData.BindGroup.SetResourcesDrawInfos(resources.DrawInfos[index], index);

                    resources.DrawInfos[index] = graph.Upload(resources.DrawInfos[index], SceneBucketDrawInfo{});
                }
            }

            passData.Dispatch = graph.ReadBuffer(dispatch, ResourceAccessFlags::Indirect);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("CreateDrawCommands")
            GPU_PROFILE_FRAME("CreateDrawCommands")

            struct PushConstants
            {
                u32 ViewCount{0};
                u32 BucketCount{0};
                u64 AvailableBucketsMask{~0lu};
            };
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {PushConstants{
                    .ViewCount = passData.Resources->VisibilityCount,
                    .BucketCount = bucketCount,
                    .AvailableBucketsMask = info.BucketsMask
                }}
            });
            cmd.DispatchIndirect({
                .Buffer = graph.GetBuffer(passData.Dispatch)
            });
        });
}
}
