#include "rendererpch.h"
#include "TonemappingPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/PbrTonemappingBindGroupRG.generated.h"

Passes::PbrTonemapping::PassData& Passes::PbrTonemapping::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, PbrTonemappingBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Tonemapping.Setup")

            passData.BindGroup = PbrTonemappingBindGroupRG(graph, ShaderSpecializations(
                ShaderSpecialization{"TONEMAPPING_TYPE"_hsv, (u32)info.Type}
            ));
            
            passData.Color = passData.BindGroup.SetResourcesColor(info.Color);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Tonemapping")
            GPU_PROFILE_FRAME("Tonemapping")

            auto& description = graph.GetImageDescription(passData.Color);
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
                .Invocations = {description.Width, description.Height, 1},
                .GroupSize = passData.BindGroup.GetTonemapGroupSize()
            });
        });
}

std::string_view Passes::PbrTonemapping::tonemappingTypeToString(TonemappingType type)
{
    switch (type)
    {
    case TonemappingType::Reinhard:
        return "Reinhard";
    case TonemappingType::ReinhardLuminance:
        return "ReinhardLuminance";
    case TonemappingType::Hable:
        return "Hable";
    case TonemappingType::PbrNeutral:
        return "PbrNeutral";
    case TonemappingType::Agx:
        return "Agx";
    case TonemappingType::GT7:
        return "GT7";
    default:
        return "Unknown";
    }
}
