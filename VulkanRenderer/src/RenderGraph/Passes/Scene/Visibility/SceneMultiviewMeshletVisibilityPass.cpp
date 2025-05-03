#include "SceneMultiviewMeshletVisibilityPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/SceneMultiviewMeshletVisibilityBindGroup.generated.h"
#include "RenderGraph/Passes/HiZ/HiZCommon.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::SceneMultiviewMeshletVisibility::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
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
            resources.ReferenceCommands = renderGraph.Read(resources.ReferenceCommands, Compute | Storage);
            resources.RenderObjects = renderGraph.Read(resources.RenderObjects, Compute | Storage);
            resources.RenderObjectBuckets = renderGraph.Read(resources.RenderObjectBuckets, Compute | Storage);
            resources.Meshlets = renderGraph.Read(resources.Meshlets, Compute | Storage);
            resources.MeshletHandles = renderGraph.Read(resources.MeshletHandles, Compute | Storage);
            resources.Views = renderGraph.Read(resources.Views, Compute | Uniform);

            resources.ResetMeshletCounts(graph);
            
            if (info.Stage == SceneVisibilityStage::Reocclusion)
            {
                for (u32 i = 0; i < resources.VisibilityCount; i++)
                    if (enumHasAny(multiview.View({i}).VisibilityFlags, SceneVisibilityFlags::OcclusionCull))
                        resources.Hiz[i] = graph.Read(resources.Hiz[i], Compute | Sampled);
            }

            for (u32 i = 0; i < resources.VisibilityCount; i++)
            {
                resources.RenderObjectVisibility[i] = graph.Read(resources.RenderObjectVisibility[i],
                    Compute | Storage);
                
                resources.MeshletVisibility[i] = graph.Read(resources.MeshletVisibility[i],
                    Compute | Storage);
                resources.MeshletVisibility[i] = graph.Write(resources.MeshletVisibility[i],
                    Compute | Storage);

                resources.MeshletBucketInfos[i] = graph.Read(resources.MeshletBucketInfos[i], Compute | Storage);
                resources.MeshletBucketInfos[i] = graph.Write(resources.MeshletBucketInfos[i], Compute | Storage);

                resources.MeshletInfoCounts[i] = graph.Read(resources.MeshletInfoCounts[i], Compute | Storage);
                resources.MeshletInfoCounts[i] = graph.Write(resources.MeshletInfoCounts[i], Compute | Storage);
            }

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("MeshletVisibilityPass")
            GPU_PROFILE_FRAME("MeshletVisibilityPass")

            const Shader& shader = resources.GetGraph()->GetShader();
            SceneMultiviewMeshletVisibilityShaderBindGroup bindGroup(shader);
            bindGroup.SetReferenceCommands({.Buffer = resources.GetBuffer(passData.Resources->ReferenceCommands)});
            bindGroup.SetObjects({.Buffer = resources.GetBuffer(passData.Resources->RenderObjects)});
            bindGroup.SetRenderObjectBuckets({.Buffer = resources.GetBuffer(passData.Resources->RenderObjectBuckets)});
            bindGroup.SetMeshlets({.Buffer = resources.GetBuffer(passData.Resources->Meshlets)});
            bindGroup.SetMeshletHandles({.Buffer = resources.GetBuffer(passData.Resources->MeshletHandles)});
            bindGroup.SetViews({.Buffer = resources.GetBuffer(passData.Resources->Views)});
            for (u32 i = 0; i < passData.Resources->VisibilityCount; i++)
            {
                bindGroup.SetObjectVisibility({
                    .Buffer = resources.GetBuffer(passData.Resources->RenderObjectVisibility[i])}, i);
                bindGroup.SetMeshletVisibility({
                    .Buffer = resources.GetBuffer(passData.Resources->MeshletVisibility[i])}, i);    
                bindGroup.SetMeshletInfos({
                    .Buffer = resources.GetBuffer(passData.Resources->MeshletBucketInfos[i])}, i);     
                bindGroup.SetMeshletInfoCount({
                    .Buffer = resources.GetBuffer(passData.Resources->MeshletInfoCounts[i])}, i);                
            }

            if (info.Stage == SceneVisibilityStage::Reocclusion)
            {
                bindGroup.SetSampler(HiZ::createSampler(HiZ::ReductionMode::Min));
                for (u32 i = 0; i < passData.Resources->VisibilityCount; i++)
                {
                    if (!passData.Resources->Hiz[i].IsValid())
                        continue;
                    
                    auto&& [hiz, hizDescription] = resources.GetTextureWithDescription(passData.Resources->Hiz[i]);
                    bindGroup.SetHiz({.Image = hiz}, hizDescription.Format == Format::D32_FLOAT ?
                        ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly, i);
                }
            }

            struct PushConstants
            {
                u32 MeshletCount{0};
                u32 ViewCount{0};
            };
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetFrameAllocators());
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
