#include "rendererpch.h"
#include "ExposurePass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/PbrCameraAutoExposureBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/PbrCameraExposureBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/PbrLuminanceHistogramBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/PbrLuminanceHistogramVisualizationBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/PbrLuminanceHistogramVisualizationOverlayBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/Types/ExposureFromHistogramOutputUniform.generated.h"

namespace
{
Passes::PbrCameraExposure::PassData& exposureFromParameters(StringId name, RG::Graph& renderGraph,
    const Passes::PbrCameraExposure::ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<Passes::PbrCameraExposure::PassData, PbrCameraExposureBindGroupRG>;
    
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
            
            struct PushConstants
            {
                f32 Aperture{};
                f32 ShutterTime{};
                f32 ISO{};
                f32 EVCompensation{};
            };

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(),
                .Data = {
                    PushConstants{
                        .Aperture = aperture,
                        .ShutterTime = shutterTime,
                        .ISO = iso,
                        .EVCompensation = *CVars::Get().GetF32CVar("Renderer.EVCompensation"_hsv)
                    }
                }
            });
            cmd.Dispatch({
                .Invocations = {1, 1, 1},
            });
        });
}

struct LuminanceHistogramPassData
{
    RG::Resource Color{};
    RG::Resource Bins{};
    u32 PixelCount{};
};
LuminanceHistogramPassData& calculateLuminanceHistogram(StringId name, RG::Graph& renderGraph, 
    const Passes::PbrCameraExposure::ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<LuminanceHistogramPassData, PbrLuminanceHistogramBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("LuminanceHistogram.Setup")

            passData.BindGroup = PbrLuminanceHistogramBindGroupRG(graph);
            
            auto& colorDescription = graph.GetImageDescription(info.Color);
            passData.Bins = graph.Create("LuminanceHistogramBins"_hsv, RGBufferDescription{
                .SizeBytes = *CVars::Get().GetI32CVar("Renderer.LuminanceHistogramBins"_hsv) * sizeof(u32)
            }); 
            std::vector<u32> zeroBins(*CVars::Get().GetI32CVar("Renderer.LuminanceHistogramBins"_hsv), 0);
            passData.Bins = graph.Upload(passData.Bins, zeroBins);
            
            passData.BindGroup.SetResourcesView(info.ViewInfo);
            passData.Color = passData.BindGroup.SetResourcesColor(info.Color);
            passData.Bins = passData.BindGroup.SetResourcesBins(passData.Bins);
            passData.PixelCount = colorDescription.Width * colorDescription.Height;
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("LuminanceHistogram")
            GPU_PROFILE_FRAME("LuminanceHistogram")
            
            auto& description = graph.GetImageDescription(passData.Color);
            struct PushConstants
            {
                f32 MinLogLuminance{};
                f32 LogLuminanceRangeInverse{};
            };
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {PushConstants{
                    .MinLogLuminance = *CVars::Get().GetF32CVar("Renderer.LuminanceHistogramMinLog"_hsv),
                    .LogLuminanceRangeInverse = 1.0f / 
                        (*CVars::Get().GetF32CVar("Renderer.LuminanceHistogramMaxLog"_hsv) -
                         *CVars::Get().GetF32CVar("Renderer.LuminanceHistogramMinLog"_hsv))
                }}
            });
            cmd.Dispatch({
                .Invocations = {description.Width, description.Height, 1},
                .GroupSize = passData.BindGroup.GetLuminanceHistogramGroupSize()
            });
        });
}
struct ExposureFromLuminanceHistogramPassData : Passes::PbrCameraExposure::PassData
{
    RG::Resource Bins{};
    RG::Resource Output{};
    u32 PixelCount{};
};
ExposureFromLuminanceHistogramPassData& exposureFromLuminanceHistogram(StringId name, RG::Graph& renderGraph,
    const LuminanceHistogramPassData& histogramPassData, const Passes::PbrCameraExposure::ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<ExposureFromLuminanceHistogramPassData, PbrCameraAutoExposureBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Exposure.Auto.Setup")

            passData.BindGroup = PbrCameraAutoExposureBindGroupRG(graph);
            
            passData.Output = graph.Create("Output"_hsv,
                RGBufferDescription{.SizeBytes = sizeof(gen::ExposureFromHistogramOutput)});
            
            passData.ViewInfo = passData.BindGroup.SetResourcesView(info.ViewInfo);
            passData.Bins = passData.BindGroup.SetResourcesBins(histogramPassData.Bins);
            passData.Output = passData.BindGroup.SetResourcesOutput(passData.Output);
            passData.PixelCount = histogramPassData.PixelCount;
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Exposure.Auto")
            GPU_PROFILE_FRAME("Exposure.Auto")
            
            struct PushConstants
            {
                f32 MinLogLuminance{};
                f32 LogLuminanceRange{};
                f32 AdaptationUpRate{};
                f32 AdaptationDownRate{};
                f32 EVCompensation{};
                f32 HistogramMin{};
                f32 HistogramMax{};
                u32 PixelCount{};
            };
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(),
                .Data = {PushConstants{
                    .MinLogLuminance = *CVars::Get().GetF32CVar("Renderer.LuminanceHistogramMinLog"_hsv),
                    .LogLuminanceRange = (*CVars::Get().GetF32CVar("Renderer.LuminanceHistogramMaxLog"_hsv) -
                        *CVars::Get().GetF32CVar("Renderer.LuminanceHistogramMinLog"_hsv)),
                    .AdaptationUpRate = *CVars::Get().GetF32CVar("Renderer.LuminanceExposureAdaptationUpRate"_hsv),
                    .AdaptationDownRate = *CVars::Get().GetF32CVar("Renderer.LuminanceExposureAdaptationDownRate"_hsv),
                    .EVCompensation = *CVars::Get().GetF32CVar("Renderer.EVCompensation"_hsv),
                    .HistogramMin = *CVars::Get().GetF32CVar("Renderer.ExposureHistogramMin"_hsv) / 100.0f,
                    .HistogramMax = 1.0f - *CVars::Get().GetF32CVar("Renderer.ExposureHistogramMax"_hsv) / 100.0f,
                    .PixelCount = passData.PixelCount
                }}});
            cmd.Dispatch({
                .Invocations = {1, 1, 1},
            });
        });
}

struct VisualizeLuminanceHistogramPassData
{
    RG::Resource Color{};
};
VisualizeLuminanceHistogramPassData& visualizeLuminanceHistogram(StringId name, RG::Graph& renderGraph,
    const ExposureFromLuminanceHistogramPassData& exposureFromHistogramPassData, 
    const Passes::PbrCameraExposure::LuminanceHistogramVisualizationInfo& visualizationInfo)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<VisualizeLuminanceHistogramPassData,
        PbrLuminanceHistogramVisualizationBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Exposure.VisualizeLuminanceHistogram.Setup")

            passData.BindGroup = PbrLuminanceHistogramVisualizationBindGroupRG(graph);
            
            passData.Color = graph.Create("HistogramVisualization"_hsv, RGImageDescription{
                .Width = (f32)visualizationInfo.Width,
                .Height = (f32)visualizationInfo.Height,
                .Format = Format::RGBA16_FLOAT,
            });
            
            passData.BindGroup.SetResourcesBins(exposureFromHistogramPassData.Bins);
            passData.BindGroup.SetResourcesHistogramOutput(exposureFromHistogramPassData.Output);
            passData.Color = passData.BindGroup.SetResourcesColor(passData.Color);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Exposure.VisualizeLuminanceHistogram")
            GPU_PROFILE_FRAME("Exposure.VisualizeLuminanceHistogram")
            
            auto& description = graph.GetImageDescription(passData.Color);
            struct PushConstants
            {
                f32 MinLogLuminance{};
                f32 LogLuminanceRange{};
                f32 EVCompensation{};
                u32 PixelCount{};
            };
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(),
                .Data = {PushConstants{
                    .MinLogLuminance = *CVars::Get().GetF32CVar("Renderer.LuminanceHistogramMinLog"_hsv),
                    .LogLuminanceRange = (*CVars::Get().GetF32CVar("Renderer.LuminanceHistogramMaxLog"_hsv) -
                        *CVars::Get().GetF32CVar("Renderer.LuminanceHistogramMinLog"_hsv)),
                    .EVCompensation = *CVars::Get().GetF32CVar("Renderer.EVCompensation"_hsv),
                    .PixelCount = exposureFromHistogramPassData.PixelCount
                }}});
            cmd.Dispatch({
                .Invocations = {description.Width, description.Height, 1},
                .GroupSize = passData.BindGroup.GetVisualizeLuminanceHistogramGroupSize()
            });
        });
}
VisualizeLuminanceHistogramPassData& visualizeLuminanceHistogramOverlay(StringId name, RG::Graph& renderGraph,
    const ExposureFromLuminanceHistogramPassData& exposureFromHistogramPassData, 
    RG::Resource sceneColor)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<VisualizeLuminanceHistogramPassData,
        PbrLuminanceHistogramVisualizationOverlayBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Exposure.VisualizeLuminanceHistogramOverlay.Setup")

            passData.BindGroup = PbrLuminanceHistogramVisualizationOverlayBindGroupRG(graph);
            
            passData.BindGroup.SetResourcesBins(exposureFromHistogramPassData.Bins);
            passData.BindGroup.SetResourcesHistogramOutput(exposureFromHistogramPassData.Output);
            passData.Color = passData.BindGroup.SetResourcesColor(sceneColor);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Exposure.VisualizeLuminanceHistogramOverlay")
            GPU_PROFILE_FRAME("Exposure.VisualizeLuminanceHistogramOverlay")
            
            auto& description = graph.GetImageDescription(passData.Color);
            struct PushConstants
            {
                f32 MinLogLuminance{};
                f32 LogLuminanceRange{};
                f32 EVCompensation{};
                u32 PixelCount{};
            };
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(),
                .Data = {PushConstants{
                    .MinLogLuminance = *CVars::Get().GetF32CVar("Renderer.LuminanceHistogramMinLog"_hsv),
                    .LogLuminanceRange = (*CVars::Get().GetF32CVar("Renderer.LuminanceHistogramMaxLog"_hsv) -
                        *CVars::Get().GetF32CVar("Renderer.LuminanceHistogramMinLog"_hsv)),
                    .EVCompensation = *CVars::Get().GetF32CVar("Renderer.EVCompensation"_hsv),
                    .PixelCount = description.Width * description.Height
                }}});
            cmd.Dispatch({
                .Invocations = {description.Width, description.Height, 1},
                .GroupSize = passData.BindGroup.GetVisualizeLuminanceHistogramOverlayGroupSize()
            });
        });
}
}

Passes::PbrCameraExposure::PassData& Passes::PbrCameraExposure::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    if (info.ExposureSettings->UseAutomaticExposure && info.Color.IsValid())
    {
        auto& histogram = calculateLuminanceHistogram(name.Concatenate(".LuminanceHistogram"_hsv), renderGraph, info);
        auto& exposure = exposureFromLuminanceHistogram(name, renderGraph, histogram, info);
        
        RG::Resource visualization = {};
        if (info.ExposureSettings->Visualize)
            visualization = info.ExposureSettings->VisualizationInfo.AsOverlay ?
                visualizeLuminanceHistogramOverlay(name.Concatenate(".VisualizeLuminanceHistogramOverlay"_hsv),
                    renderGraph, exposure, info.Color).Color :
                visualizeLuminanceHistogram(name.Concatenate(".VisualizeLuminanceHistogram"_hsv),
                    renderGraph, exposure, info.ExposureSettings->VisualizationInfo).Color;
    
        exposure.HistogramVisualization = visualization;
        
        return exposure;
    }
    return exposureFromParameters(name, renderGraph, info);
}

f32 Passes::PbrCameraExposure::convertEV100ToExposure(f32 ev100)
{
    const f32 maxLuminance = 1.2f * pow(2.0f, ev100);

    return 1.0f / maxLuminance;
}
