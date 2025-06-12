#include "SceneMultiviewMeshletVisibilityPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/SceneMultiviewMeshletVisibilityBindGroup.generated.h"
#include "RenderGraph/Passes/HiZ/HiZCommon.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::SceneMultiviewMeshletVisibility::PassData& Passes::SceneMultiviewMeshletVisibility::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("MeshletVisibilityPass.Setup")

            graph.SetShader("scene-multiview-meshlet-visibility"_hsv,
                ShaderSpecializations{
                    ShaderSpecialization{"REOCCLUSION"_hsv, info.Stage == SceneVisibilityStage::Reocclusion}});
            
            auto& multiview = *info.MultiviewVisibility;
            
            passData.Resources = info.Resources;
            auto& resources = *passData.Resources;
            resources.ReferenceCommands = renderGraph.ReadBuffer(resources.ReferenceCommands, Compute | Storage);
            resources.RenderObjects = renderGraph.ReadBuffer(resources.RenderObjects, Compute | Storage);
            resources.RenderObjectBuckets = renderGraph.ReadBuffer(resources.RenderObjectBuckets, Compute | Storage);
            resources.Meshlets = renderGraph.ReadBuffer(resources.Meshlets, Compute | Storage);
            resources.MeshletHandles = renderGraph.ReadBuffer(resources.MeshletHandles, Compute | Storage);

            resources.ResetMeshletCounts(graph);
            
            if (info.Stage == SceneVisibilityStage::Reocclusion)
            {
                for (u32 i = 0; i < resources.VisibilityCount; i++)
                    if (enumHasAny(multiview.View({i}).ViewInfo.Camera.VisibilityFlags, VisibilityFlags::OcclusionCull))
                        resources.Hiz[i] = graph.ReadImage(resources.Hiz[i], Compute | Sampled);
            }

            for (u32 i = 0; i < resources.VisibilityCount; i++)
            {
                resources.Views[i] = renderGraph.ReadBuffer( resources.Views[i], Compute | Uniform);
                
                resources.RenderObjectVisibility[i] = graph.ReadBuffer(resources.RenderObjectVisibility[i],
                    Compute | Storage);
                resources.MeshletVisibility[i] = graph.ReadWriteBuffer(resources.MeshletVisibility[i],
                    Compute | Storage);
                resources.MeshletBucketInfos[i] = graph.ReadWriteBuffer(resources.MeshletBucketInfos[i],
                    Compute | Storage);
                resources.MeshletInfoCounts[i] = graph.ReadWriteBuffer(resources.MeshletInfoCounts[i],
                    Compute | Storage);
            }
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("MeshletVisibilityPass")
            GPU_PROFILE_FRAME("MeshletVisibilityPass")

            const Shader& shader = graph.GetShader();
            SceneMultiviewMeshletVisibilityShaderBindGroup bindGroup(shader);
            bindGroup.SetReferenceCommands(graph.GetBufferBinding(passData.Resources->ReferenceCommands));
            bindGroup.SetObjects(graph.GetBufferBinding(passData.Resources->RenderObjects));
            bindGroup.SetRenderObjectBuckets(graph.GetBufferBinding(passData.Resources->RenderObjectBuckets));
            bindGroup.SetMeshlets(graph.GetBufferBinding(passData.Resources->Meshlets));
            bindGroup.SetMeshletHandles(graph.GetBufferBinding(passData.Resources->MeshletHandles));
            for (u32 i = 0; i < passData.Resources->VisibilityCount; i++)
            {
                bindGroup.SetViews(graph.GetBufferBinding(passData.Resources->Views[i]), i);
                bindGroup.SetObjectVisibility(
                    graph.GetBufferBinding(passData.Resources->RenderObjectVisibility[i]), i);
                bindGroup.SetMeshletVisibility(
                    graph.GetBufferBinding(passData.Resources->MeshletVisibility[i]), i);    
                bindGroup.SetMeshletInfos(
                    graph.GetBufferBinding(passData.Resources->MeshletBucketInfos[i]), i);     
                bindGroup.SetMeshletInfoCount(
                    graph.GetBufferBinding(passData.Resources->MeshletInfoCounts[i]), i);                
            }

            if (info.Stage == SceneVisibilityStage::Reocclusion)
            {
                bindGroup.SetSampler(HiZ::createSampler(HiZ::ReductionMode::Min));
                for (u32 i = 0; i < passData.Resources->VisibilityCount; i++)
                {
                    if (!passData.Resources->Hiz[i].IsValid())
                        continue;
                    
                    bindGroup.SetHiz(graph.GetImageBinding(passData.Resources->Hiz[i]), i);
                }
            }

            struct PushConstants
            {
                u32 MeshletCount{0};
                u32 ViewCount{0};
            };
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {PushConstants{
                    .MeshletCount = passData.Resources->MeshletCount,
                    .ViewCount = passData.Resources->VisibilityCount}}});
            cmd.Dispatch({
               .Invocations = {passData.Resources->MeshletCount, 1, 1},
               .GroupSize = {64, 1, 1}});
        });
}
