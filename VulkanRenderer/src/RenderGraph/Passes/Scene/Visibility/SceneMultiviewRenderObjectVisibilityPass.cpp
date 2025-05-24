#include "SceneMultiviewRenderObjectVisibilityPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/SceneMultiviewRenderObjectVisibilityBindGroup.generated.h"
#include "RenderGraph/Passes/HiZ/HiZCommon.h"

Passes::SceneMultiviewRenderObjectVisibility::PassData& Passes::SceneMultiviewRenderObjectVisibility::addToGraph(
    StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name, 
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("RenderObjectVisibilityPass.Setup")

            graph.SetShader("scene-multiview-render-object-visibility"_hsv,
                ShaderSpecializations{
                    ShaderSpecialization{"REOCCLUSION"_hsv, info.Stage == SceneVisibilityStage::Reocclusion}});
            
            auto& multiview = *info.MultiviewVisibility;
            
            passData.Resources = info.Resources;
            auto& resources = *passData.Resources;
            resources.RenderObjects = renderGraph.ReadBuffer(resources.RenderObjects, Compute | Storage);
            resources.RenderObjectHandles = renderGraph.ReadBuffer(resources.RenderObjectHandles, Compute | Storage);
            resources.Views = renderGraph.ReadBuffer(resources.Views, Compute | Uniform);

            if (info.Stage != SceneVisibilityStage::Reocclusion)
            {
                resources.UploadViews(multiview, graph);
            }
            else
            {
                for (u32 i = 0; i < resources.VisibilityCount; i++)
                    if (enumHasAny(multiview.View({i}).VisibilityFlags, SceneVisibilityFlags::OcclusionCull))
                        resources.Hiz[i] = graph.ReadImage(resources.Hiz[i], Compute | Sampled);
            }

            for (u32 i = 0; i < resources.VisibilityCount; i++)
            {
                resources.RenderObjectVisibility[i] = graph.ReadWriteBuffer(resources.RenderObjectVisibility[i],
                    Compute | Storage);
            }
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("RenderObjectVisibilityPass")
            GPU_PROFILE_FRAME("RenderObjectVisibilityPass")

            const Shader& shader = graph.GetShader();
            SceneMultiviewRenderObjectVisibilityShaderBindGroup bindGroup(shader);
            bindGroup.SetObjects(graph.GetBufferBinding(passData.Resources->RenderObjects));
            bindGroup.SetObjectHandles(graph.GetBufferBinding(passData.Resources->RenderObjectHandles));
            bindGroup.SetViews(graph.GetBufferBinding(passData.Resources->Views));
            for (u32 i = 0; i < passData.Resources->VisibilityCount; i++)
                bindGroup.SetObjectVisibility({
                    .Buffer = graph.GetBuffer(passData.Resources->RenderObjectVisibility[i])}, i);

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
                u32 RenderObjectCount{0};
                u32 ViewCount{0};
            };
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {PushConstants{
                    .RenderObjectCount = passData.Resources->RenderObjectCount,
                    .ViewCount = passData.Resources->VisibilityCount}}});
            cmd.Dispatch({
               .Invocations = {passData.Resources->RenderObjectCount, 1, 1},
               .GroupSize = {64, 1, 1}});
        });
}
