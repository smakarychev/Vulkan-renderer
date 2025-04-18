#include "SceneMultiviewRenderObjectVisibilityPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/SceneMultiviewRenderObjectVisibilityBindGroup.generated.h"

RG::Pass& Passes::SceneMultiviewRenderObjectVisibility::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name, 
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("RenderObjectVisibilityPass.Setup")

            graph.SetShader("scene-multiview-render-object-visibility.shader",
                ShaderSpecializations{
                    ShaderSpecialization{"REOCCLUSION"_hsv, info.Stage == SceneVisibilityStage::Reocclusion}});
            
            auto& multiview = *info.MultiviewVisibility;
            
            passData.Resources = info.Resources;
            auto& resources = *passData.Resources;
            resources.RenderObjects = renderGraph.Read(resources.RenderObjects, Compute | Storage);
            resources.RenderObjectHandles = renderGraph.Read(resources.RenderObjectHandles, Compute | Storage);
            resources.Views = renderGraph.Read(resources.Views, Compute | Uniform);

            if (info.Stage != SceneVisibilityStage::Reocclusion)
            {
                resources.UploadViews(multiview, graph);
            }
            else
            {
                for (u32 i = 0; i < resources.VisibilityCount; i++)
                    if (enumHasAny(multiview.View({i}).VisibilityFlags, SceneVisibilityFlags::OcclusionCull))
                        resources.Hiz[i] = graph.Read(resources.Hiz[i], Compute | Sampled);
            }

            for (u32 i = 0; i < resources.VisibilityCount; i++)
            {
                resources.RenderObjectVisibility[i] = graph.Read(resources.RenderObjectVisibility[i],
                    Compute | Storage);
                resources.RenderObjectVisibility[i] = graph.Write(resources.RenderObjectVisibility[i],
                    Compute | Storage);
            }

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("RenderObjectVisibilityPass")
            GPU_PROFILE_FRAME("RenderObjectVisibilityPass")

            const Shader& shader = resources.GetGraph()->GetShader();
            SceneMultiviewRenderObjectVisibilityShaderBindGroup bindGroup(shader);
            bindGroup.SetObjects({.Buffer = resources.GetBuffer(passData.Resources->RenderObjects)});
            bindGroup.SetObjectHandles({.Buffer = resources.GetBuffer(passData.Resources->RenderObjectHandles)});
            bindGroup.SetViews({.Buffer = resources.GetBuffer(passData.Resources->Views)});
            for (u32 i = 0; i < passData.Resources->VisibilityCount; i++)
                bindGroup.SetObjectVisibility({
                    .Buffer = resources.GetBuffer(passData.Resources->RenderObjectVisibility[i])}, i);

            struct PushConstants
            {
                u32 RenderObjectCount{0};
                u32 ViewCount{0};
            };
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
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
