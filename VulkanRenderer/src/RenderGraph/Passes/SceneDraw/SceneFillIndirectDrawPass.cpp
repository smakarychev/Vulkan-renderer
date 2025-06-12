#include "SceneFillIndirectDrawPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/SceneFillIndirectDrawsBindGroup.generated.h"
#include "Scene/SceneRenderObjectSet.h"

Passes::SceneFillIndirectDraw::PassData& Passes::SceneFillIndirectDraw::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate : PassData
    {
        Resource ReferenceCommands{};
        Resource MeshletInfos{};
        Resource MeshletInfoCount{};

        u32 BucketCount{0};
        u32 CommandCount{0};
    };
    
    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Scene.SceneFillIndirectDraw.Setup")

            graph.SetShader("scene-fill-indirect-draws"_hsv);

            passData.BucketCount = info.BucketCount;
            passData.CommandCount = info.Geometry->CommandCount;

            passData.ReferenceCommands = graph.Import("ReferenceCommands"_hsv,
                info.Geometry->Commands.Buffer);
            passData.ReferenceCommands = graph.ReadBuffer(passData.ReferenceCommands, Compute | Storage);

            passData.MeshletInfos = graph.ReadBuffer(info.MeshletInfos, Compute | Storage);
            passData.MeshletInfoCount = graph.ReadBuffer(info.MeshletInfoCount, Compute | Uniform);

            for (u32 i = 0; i < passData.BucketCount; i++)
            {
                if (!info.Draws[i].IsValid())
                    continue;
                
                passData.Draws[i] = graph.ReadWriteBuffer(info.Draws[i], Compute | Storage);
                passData.DrawInfos[i] = graph.ReadWriteBuffer(info.DrawInfos[i], Compute | Storage);
                passData.DrawInfos[i] = graph.Upload(passData.DrawInfos[i], SceneBucketDrawInfo{});
            }
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.SceneFillIndirectDraw")
            GPU_PROFILE_FRAME("Scene.SceneFillIndirectDraw")

            const Shader& shader = graph.GetShader();
            SceneFillIndirectDrawsShaderBindGroup bindGroup(shader);
            bindGroup.SetReferenceCommands(graph.GetBufferBinding(passData.ReferenceCommands));
            bindGroup.SetMeshletInfos(graph.GetBufferBinding(passData.MeshletInfos));
            bindGroup.SetMeshletInfoCount(graph.GetBufferBinding(passData.MeshletInfoCount));
            u64 availableBucketMask = 0;
            for (u32 bucketIndex = 0; bucketIndex < passData.BucketCount; bucketIndex++)
            {
                if (!passData.Draws[bucketIndex].IsValid())
                    continue;
                availableBucketMask |= (1llu << bucketIndex);
                bindGroup.SetDrawCommands(graph.GetBufferBinding(passData.Draws[bucketIndex]), bucketIndex);
                bindGroup.SetDrawInfo(graph.GetBufferBinding(passData.DrawInfos[bucketIndex]), bucketIndex);
            }

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            /* todo: this can use indirect dispatch */
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {availableBucketMask}});
            cmd.Dispatch({
               .Invocations = {passData.CommandCount, 1, 1},
               .GroupSize = {256, 1, 1}});
        });
}
