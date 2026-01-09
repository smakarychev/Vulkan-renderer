#include "rendererpch.h"

#include "SceneFillIndirectDrawPass.h"

#include "RenderGraph/Passes/Generated/SceneFillIndirectDrawsBindGroupRG.generated.h"

Passes::SceneFillIndirectDraw::PassData& Passes::SceneFillIndirectDraw::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, SceneFillIndirectDrawsBindGroupRG>;

    const u32 bucketCount = info.BucketCount;
    const u32 commandCount = info.Geometry->CommandCount;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Scene.SceneFillIndirectDraw.Setup")

            passData.BindGroup = SceneFillIndirectDrawsBindGroupRG(graph);

            for (u32 i = 0; i < bucketCount; i++)
            {
                if (!info.Draws[i].IsValid())
                    continue;
                
                passData.Draws[i] = passData.BindGroup.SetResourcesDrawCommands(info.Draws[i], i);
                passData.DrawInfos[i] = passData.BindGroup.SetResourcesDrawInfos(info.DrawInfos[i], i);
                passData.DrawInfos[i] = graph.Upload(passData.DrawInfos[i], SceneBucketDrawInfo{});
            }

            passData.BindGroup.SetResourcesReferenceCommands(graph.Import("ReferenceCommands"_hsv,
                info.Geometry->Commands.Buffer));
            passData.BindGroup.SetResourcesMeshletInfos(info.MeshletInfos);
            passData.BindGroup.SetResourcesMeshletInfoCounts(info.MeshletInfoCount);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.SceneFillIndirectDraw")
            GPU_PROFILE_FRAME("Scene.SceneFillIndirectDraw")

            u64 availableBucketMask = 0;
            for (u32 bucketIndex = 0; bucketIndex < bucketCount; bucketIndex++)
            {
                if (!passData.Draws[bucketIndex].IsValid())
                    continue;
                availableBucketMask |= (1llu << bucketIndex);
            }

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
            /* todo: this can use indirect dispatch */
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {availableBucketMask}});
            cmd.Dispatch({
               .Invocations = {commandCount, 1, 1},
               .GroupSize = passData.BindGroup.GetFillIndirectDrawsGroupSize()
            });
        });
}
