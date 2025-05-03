#include "PrepareVisibleMeshletInfoPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/ScenePrepareVisibleMeshletInfoBindGroup.generated.h"
#include "Scene/SceneRenderObjectSet.h"

RG::Pass& Passes::PrepareVisibleMeshletInfo::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate
    {
        Resource ReferenceCommands{};
        Resource Buckets{};
        Resource MeshletHandles{};
        Resource MeshletInfos{};
        Resource MeshletInfoCount{};

        u32 MeshletCount{0};
    };
    
    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Scene.PrepareVisibleMeshletInfo.Setup")

            graph.SetShader("scene-prepare-visible-meshlet-info"_hsv);

            passData.MeshletCount = info.RenderObjectSet->MeshletCount();

            passData.ReferenceCommands = graph.AddExternal("ReferenceCommands"_hsv,
                info.RenderObjectSet->Geometry().Commands.Buffer);
            passData.ReferenceCommands = graph.Read(passData.ReferenceCommands, Compute | Storage);

            passData.Buckets = graph.AddExternal("Buckets"_hsv,
                info.RenderObjectSet->BucketBits());
            passData.Buckets = graph.Read(passData.Buckets, Compute | Storage);
            
            passData.MeshletHandles = graph.AddExternal("MeshletHandles"_hsv,
                info.RenderObjectSet->MeshletHandles());
            passData.MeshletHandles = graph.Read(passData.MeshletHandles, Compute | Storage);

            passData.MeshletInfos = graph.CreateResource("MeshletInfos"_hsv,
                GraphBufferDescription{
                    .SizeBytes = sizeof(SceneMeshletBucketInfo) * passData.MeshletCount});
            passData.MeshletInfos = graph.Write(passData.MeshletInfos, Compute | Storage);

            passData.MeshletInfoCount = graph.CreateResource("MeshletInfoCount"_hsv,
                GraphBufferDescription{.SizeBytes = sizeof(u32)});
            passData.MeshletInfoCount = graph.Read(passData.MeshletInfoCount, Compute | Storage);
            passData.MeshletInfoCount = graph.Write(passData.MeshletInfoCount, Compute | Storage);
            graph.Upload(passData.MeshletInfoCount, 0);

            PassData passDataPublic = {};
            passDataPublic.MeshletInfos = passData.MeshletInfos;
            passDataPublic.MeshletInfoCount = passData.MeshletInfoCount;

            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Scene.PrepareVisibleMeshletInfo")
            GPU_PROFILE_FRAME("Scene.PrepareVisibleMeshletInfo")

            const Shader& shader = resources.GetGraph()->GetShader();
            ScenePrepareVisibleMeshletInfoShaderBindGroup bindGroup(shader);
            bindGroup.SetReferenceCommands({.Buffer = resources.GetBuffer(passData.ReferenceCommands)});
            bindGroup.SetRenderObjectBuckets({.Buffer = resources.GetBuffer(passData.Buckets)});
            bindGroup.SetMeshletHandles({.Buffer = resources.GetBuffer(passData.MeshletHandles)});
            bindGroup.SetMeshletInfos({.Buffer = resources.GetBuffer(passData.MeshletInfos)});
            bindGroup.SetMeshletInfoCount({.Buffer = resources.GetBuffer(passData.MeshletInfoCount)});

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {passData.MeshletCount}});
            cmd.Dispatch({
               .Invocations = {passData.MeshletCount, 1, 1},
               .GroupSize = {64, 1, 1}});
        });
}
