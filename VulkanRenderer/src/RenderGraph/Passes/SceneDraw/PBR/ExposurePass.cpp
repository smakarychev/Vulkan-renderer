#include "rendererpch.h"
#include "ExposurePass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/PbrCameraExposureBindGroupRG.generated.h"

Passes::PbrCameraExposure::PassData& Passes::PbrCameraExposure::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, PbrCameraExposureBindGroupRG>;
    
    const f32 aperture = info.ExposureSettings->Aperture;
    const f32 shutterTime = info.ExposureSettings->ShutterTime;
    const f32 iso = info.ExposureSettings->ISO;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Exposure.Setup")

            passData.BindGroup = PbrCameraExposureBindGroupRG(graph);
            
            passData.ViewInfo = passData.BindGroup.SetResourcesView(info.ViewInfo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Exposure")
            GPU_PROFILE_FRAME("Exposure")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {{aperture, shutterTime, iso}}
            });
            cmd.Dispatch({
               .Invocations = {1, 1, 1},
            });
        });
}
