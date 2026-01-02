#include "rendererpch.h"

#include "ShadowCamerasGpuPass.h"

#include "ViewInfoGPU.h"
#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/Passes/Generated/CreateShadowCamerasBindGroupRG.generated.h"
#include "RenderGraph/Passes/SceneDraw/Shadow/SceneCsmPass.h"

#include "Rendering/Shader/ShaderCache.h"

Passes::ShadowCamerasGpu::PassData& Passes::ShadowCamerasGpu::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    using PassDataBind = PassDataWithBind<PassData, CreateShadowCamerasBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("ShadowCameras.GPU.Setup")

            passData.BindGroup = CreateShadowCamerasBindGroupRG(graph);

            const Resource csmData = graph.Create("CSM.Data"_hsv, RGBufferDescription{
                .SizeBytes = sizeof(SceneCsm::CsmInfo)});

            passData.ViewInfo = passData.BindGroup.SetResourcesView(info.View);
            passData.DepthMinMax = passData.BindGroup.SetResourcesMinMax(info.DepthMinMax);
            passData.CsmData = passData.BindGroup.SetResourcesCsm(csmData);
            for (u32 i = 0; i < passData.ShadowViews.size(); i++)
            {
                passData.ShadowViews[i] = graph.Create("CSM.ShadowView"_hsv, RGBufferDescription{
                    .SizeBytes = sizeof(ViewInfoGPU)});
                passData.ShadowViews[i] = passData.BindGroup.SetResourcesShadowViews(passData.ShadowViews[i], i);
            }
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("ShadowCameras.GPU")
            GPU_PROFILE_FRAME("ShadowCameras.GPU")

            struct PushConstant
            {
                u32 ShadowSize;
                u32 CascadeCount;
                f32 MaxShadowDistance;
                glm::vec3 LightDirection;
            };
            PushConstant pushConstant = {
                .ShadowSize = SHADOW_MAP_RESOLUTION,
                .CascadeCount = SHADOW_CASCADES,
                .MaxShadowDistance = *CVars::Get().GetF32CVar("Renderer.Limits.MaxShadowDistance"_hsv),
                .LightDirection = info.LightDirection
            };

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
            	.Data = {pushConstant}});
            cmd.Dispatch({
                .Invocations = {SHADOW_CASCADES, 1, 1},
                .GroupSize = passData.BindGroup.GetMainGroupSize()
            });
        });
}
