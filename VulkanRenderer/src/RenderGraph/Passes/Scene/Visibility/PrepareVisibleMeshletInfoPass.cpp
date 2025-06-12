#include "PrepareVisibleMeshletInfoPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/ScenePrepareVisibleMeshletInfoBindGroup.generated.h"
#include "Scene/SceneRenderObjectSet.h"

Passes::PrepareVisibleMeshletInfo::PassData& Passes::PrepareVisibleMeshletInfo::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate : PassData
    {
        Resource ReferenceCommands{};
        Resource Buckets{};
        Resource MeshletHandles{};

        u32 MeshletCount{0};
    };
    
    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Scene.PrepareVisibleMeshletInfo.Setup")

            graph.SetShader("scene-prepare-visible-meshlet-info"_hsv);

            passData.MeshletCount = info.RenderObjectSet->MeshletCount();

            passData.ReferenceCommands = graph.Import("ReferenceCommands"_hsv,
                info.RenderObjectSet->Geometry().Commands.Buffer);
            passData.ReferenceCommands = graph.ReadBuffer(passData.ReferenceCommands, Compute | Storage);

            passData.Buckets = graph.Import("Buckets"_hsv,
                info.RenderObjectSet->BucketBits());
            passData.Buckets = graph.ReadBuffer(passData.Buckets, Compute | Storage);
            
            passData.MeshletHandles = graph.Import("MeshletHandles"_hsv,
                info.RenderObjectSet->MeshletHandles());
            passData.MeshletHandles = graph.ReadBuffer(passData.MeshletHandles, Compute | Storage);

            passData.MeshletInfos = graph.Create("MeshletInfos"_hsv,
                RGBufferDescription{
                    .SizeBytes = sizeof(SceneMeshletBucketInfo) * passData.MeshletCount});
            passData.MeshletInfos = graph.ReadBuffer(passData.MeshletInfos, Compute | Storage);

            passData.MeshletInfoCount = graph.Create("MeshletInfoCount"_hsv,
                RGBufferDescription{.SizeBytes = sizeof(u32)});
            passData.MeshletInfoCount = graph.ReadWriteBuffer(passData.MeshletInfoCount, Compute | Storage);
            passData.MeshletInfoCount = graph.Upload(passData.MeshletInfoCount, 0);
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.PrepareVisibleMeshletInfo")
            GPU_PROFILE_FRAME("Scene.PrepareVisibleMeshletInfo")

            const Shader& shader = graph.GetShader();
            ScenePrepareVisibleMeshletInfoShaderBindGroup bindGroup(shader);
            bindGroup.SetReferenceCommands(graph.GetBufferBinding(passData.ReferenceCommands));
            bindGroup.SetRenderObjectBuckets(graph.GetBufferBinding(passData.Buckets));
            bindGroup.SetMeshletHandles(graph.GetBufferBinding(passData.MeshletHandles));
            bindGroup.SetMeshletInfos(graph.GetBufferBinding(passData.MeshletInfos));
            bindGroup.SetMeshletInfoCount(graph.GetBufferBinding(passData.MeshletInfoCount));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {passData.MeshletCount}});
            cmd.Dispatch({
               .Invocations = {passData.MeshletCount, 1, 1},
               .GroupSize = {64, 1, 1}});
        });
}
